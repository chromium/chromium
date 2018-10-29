// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_entry_impl.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <string>
#include <utility>

#include "base/containers/queue.h"
#include "base/files/file_path.h"
#include "base/i18n/rtl.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/url_formatter/url_formatter.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/common/content_constants_internal.h"
#include "content/common/navigation_params.h"
#include "content/common/page_state_serialization.h"
#include "content/public/browser/reload_type.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/url_constants.h"
#include "ui/gfx/text_elider.h"

#if defined(OS_ANDROID)
#include "base/android/content_uri_utils.h"
#endif

using base::UTF16ToUTF8;

namespace content {

namespace {

// Use this to get a new unique ID for a NavigationEntry during construction.
// The returned ID is guaranteed to be nonzero (which is the "no ID" indicator).
int CreateUniqueEntryID() {
  static int unique_id_counter = 0;
  return ++unique_id_counter;
}

void RecursivelyGenerateFrameEntries(
    const ExplodedFrameState& state,
    const std::vector<base::Optional<base::string16>>& referenced_files,
    NavigationEntryImpl::TreeNode* node) {
  // Set a single-frame PageState on the entry.
  ExplodedPageState page_state;

  // Copy the incoming PageState's list of referenced files into the main
  // frame's PageState.  (We don't pass the list to subframes below.)
  // TODO(creis): Grant access to this list for each process that renders
  // this page, even for OOPIFs.  Eventually keep track of a verified list of
  // files per frame, so that we only grant access to processes that need it.
  if (referenced_files.size() > 0)
    page_state.referenced_files = referenced_files;

  page_state.top = state;
  std::string data;
  EncodePageState(page_state, &data);
  DCHECK(!data.empty()) << "Shouldn't generate an empty PageState.";

  node->frame_entry = new FrameNavigationEntry(
      UTF16ToUTF8(state.target.value_or(base::string16())),
      state.item_sequence_number, state.document_sequence_number, nullptr,
      nullptr, GURL(state.url_string.value_or(base::string16())),
      Referrer(GURL(state.referrer.value_or(base::string16())),
               state.referrer_policy),
      std::vector<GURL>(), PageState::CreateFromEncodedData(data), "GET", -1,
      nullptr /* blob_url_loader_factory */);

  // Don't pass the file list to subframes, since that would result in multiple
  // copies of it ending up in the combined list in GetPageState (via
  // RecursivelyGenerateFrameState).
  std::vector<base::Optional<base::string16>> empty_file_list;

  for (const ExplodedFrameState& child_state : state.children) {
    node->children.push_back(
        std::make_unique<NavigationEntryImpl::TreeNode>(node, nullptr));
    RecursivelyGenerateFrameEntries(child_state, empty_file_list,
                                    node->children.back().get());
  }
}

void RecursivelyGenerateFrameState(
    NavigationEntryImpl::TreeNode* node,
    ExplodedFrameState* state,
    std::vector<base::Optional<base::string16>>* referenced_files) {
  // The FrameNavigationEntry's PageState contains just the ExplodedFrameState
  // for that particular frame.
  ExplodedPageState exploded_page_state;
  if (!DecodePageState(node->frame_entry->page_state().ToEncodedData(),
                       &exploded_page_state)) {
    NOTREACHED();
    return;
  }
  ExplodedFrameState frame_state = exploded_page_state.top;

  // Copy the FrameNavigationEntry's frame state into the destination state.
  *state = frame_state;

  // Copy the frame's files into the PageState's |referenced_files|.
  referenced_files->reserve(referenced_files->size() +
                            exploded_page_state.referenced_files.size());
  for (auto& file : exploded_page_state.referenced_files)
    referenced_files->emplace_back(file);

  state->children.resize(node->children.size());
  for (size_t i = 0; i < node->children.size(); ++i) {
    RecursivelyGenerateFrameState(node->children[i].get(), &state->children[i],
                                  referenced_files);
  }
}

// Walk the ancestor chain for both the |frame_tree_node| and the |node|.
// Comparing the inputs directly is not performed, as this method assumes they
// already match each other. Returns false if a mismatch in unique name or
// ancestor chain is detected, otherwise true.
bool InSameTreePosition(FrameTreeNode* frame_tree_node,
                        NavigationEntryImpl::TreeNode* node) {
  FrameTreeNode* ftn = frame_tree_node->parent();
  NavigationEntryImpl::TreeNode* current_node = node->parent;
  while (ftn && current_node) {
    if (!current_node->MatchesFrame(ftn))
      return false;

    if ((!current_node->parent && ftn->parent()) ||
        (current_node->parent && !ftn->parent())) {
      return false;
    }

    ftn = ftn->parent();
    current_node = current_node->parent;
  }
  return true;
}

}  // namespace

NavigationEntryImpl::TreeNode::TreeNode(TreeNode* parent,
                                        FrameNavigationEntry* frame_entry)
    : parent(parent), frame_entry(frame_entry) {}

NavigationEntryImpl::TreeNode::~TreeNode() {
}

bool NavigationEntryImpl::TreeNode::MatchesFrame(
    FrameTreeNode* frame_tree_node) const {
  // The root node is for the main frame whether the unique name matches or not.
  if (!parent)
    return frame_tree_node->IsMainFrame();

  // Otherwise check the unique name for subframes.
  return !frame_tree_node->IsMainFrame() &&
         frame_tree_node->unique_name() == frame_entry->frame_unique_name();
}

std::unique_ptr<NavigationEntryImpl::TreeNode>
NavigationEntryImpl::TreeNode::CloneAndReplace(
    FrameNavigationEntry* frame_navigation_entry,
    bool clone_children_of_target,
    FrameTreeNode* target_frame_tree_node,
    FrameTreeNode* current_frame_tree_node,
    TreeNode* parent_node) const {
  // Clone this TreeNode, possibly replacing its FrameNavigationEntry.
  bool is_target_frame =
      target_frame_tree_node && MatchesFrame(target_frame_tree_node);
  std::unique_ptr<NavigationEntryImpl::TreeNode> copy(
      new NavigationEntryImpl::TreeNode(
          parent_node,
          is_target_frame ? frame_navigation_entry : frame_entry->Clone()));

  // Recursively clone the children if needed.
  if (!is_target_frame || clone_children_of_target) {
    for (size_t i = 0; i < children.size(); i++) {
      const auto& child = children[i];

      // Don't check whether it's still in the tree if |current_frame_tree_node|
      // is null.
      if (!current_frame_tree_node) {
        copy->children.push_back(child->CloneAndReplace(
            frame_navigation_entry, clone_children_of_target,
            target_frame_tree_node, nullptr, copy.get()));
        continue;
      }

      // Otherwise, make sure the frame is still in the tree before cloning it.
      // This is O(N^2) in the worst case because we need to look up each frame
      // (and since we want to avoid a map of unique names, which can be very
      // long).  To partly mitigate this, we add an optimization for the common
      // case that the two child lists are the same length and are likely in the
      // same order: we pick the starting offset of the inner loop to get O(N).
      size_t ftn_child_count = current_frame_tree_node->child_count();
      for (size_t j = 0; j < ftn_child_count; j++) {
        size_t index = j;
        // If the two lists of children are the same length, start looking at
        // the same index as |child|.
        if (children.size() == ftn_child_count)
          index = (i + j) % ftn_child_count;

        if (current_frame_tree_node->child_at(index)->unique_name() ==
            child->frame_entry->frame_unique_name()) {
          // Found |child| in the tree.  Clone it and break out of inner loop.
          copy->children.push_back(child->CloneAndReplace(
              frame_navigation_entry, clone_children_of_target,
              target_frame_tree_node, current_frame_tree_node->child_at(index),
              copy.get()));
          break;
        }
      }
    }
  }

  return copy;
}

std::unique_ptr<NavigationEntry> NavigationEntry::Create() {
  return base::WrapUnique(new NavigationEntryImpl());
}

NavigationEntryImpl* NavigationEntryImpl::FromNavigationEntry(
    NavigationEntry* entry) {
  return static_cast<NavigationEntryImpl*>(entry);
}

const NavigationEntryImpl* NavigationEntryImpl::FromNavigationEntry(
    const NavigationEntry* entry) {
  return static_cast<const NavigationEntryImpl*>(entry);
}

std::unique_ptr<NavigationEntryImpl> NavigationEntryImpl::FromNavigationEntry(
    std::unique_ptr<NavigationEntry> entry) {
  return base::WrapUnique(FromNavigationEntry(entry.release()));
}

NavigationEntryImpl::NavigationEntryImpl()
    : NavigationEntryImpl(nullptr,
                          GURL(),
                          Referrer(),
                          base::string16(),
                          ui::PAGE_TRANSITION_LINK,
                          false,
                          nullptr) {}

NavigationEntryImpl::NavigationEntryImpl(
    scoped_refptr<SiteInstanceImpl> instance,
    const GURL& url,
    const Referrer& referrer,
    const base::string16& title,
    ui::PageTransition transition_type,
    bool is_renderer_initiated,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory)
    : frame_tree_(new TreeNode(
          nullptr,
          new FrameNavigationEntry("",
                                   -1,
                                   -1,
                                   std::move(instance),
                                   nullptr,
                                   url,
                                   referrer,
                                   std::vector<GURL>(),
                                   PageState(),
                                   "GET",
                                   -1,
                                   std::move(blob_url_loader_factory)))),
      unique_id_(CreateUniqueEntryID()),
      bindings_(kInvalidBindings),
      page_type_(PAGE_TYPE_NORMAL),
      update_virtual_url_with_url_(false),
      title_(title),
      transition_type_(transition_type),
      restore_type_(RestoreType::NONE),
      is_overriding_user_agent_(false),
      http_status_code_(0),
      is_renderer_initiated_(is_renderer_initiated),
      should_replace_entry_(false),
      should_clear_history_list_(false),
      can_load_local_resources_(false),
      frame_tree_node_id_(-1),
      has_user_gesture_(false),
      reload_type_(ReloadType::NONE),
      started_from_context_menu_(false),
      ssl_error_(false) {}

NavigationEntryImpl::~NavigationEntryImpl() {
}

int NavigationEntryImpl::GetUniqueID() const {
  return unique_id_;
}

PageType NavigationEntryImpl::GetPageType() const {
  return page_type_;
}

void NavigationEntryImpl::SetURL(const GURL& url) {
  frame_tree_->frame_entry->set_url(url);
  cached_display_title_.clear();
}

const GURL& NavigationEntryImpl::GetURL() const {
  return frame_tree_->frame_entry->url();
}

void NavigationEntryImpl::SetBaseURLForDataURL(const GURL& url) {
  base_url_for_data_url_ = url;
}

const GURL& NavigationEntryImpl::GetBaseURLForDataURL() const {
  return base_url_for_data_url_;
}

#if defined(OS_ANDROID)
void NavigationEntryImpl::SetDataURLAsString(
    scoped_refptr<base::RefCountedString> data_url) {
  if (data_url) {
    // A quick check that it's actually a data URL.
    DCHECK(base::StartsWith(data_url->front_as<char>(), url::kDataScheme,
                            base::CompareCase::SENSITIVE));
  }
  data_url_as_string_ = std::move(data_url);
}

const scoped_refptr<const base::RefCountedString>&
NavigationEntryImpl::GetDataURLAsString() const {
  return data_url_as_string_;
}
#endif

void NavigationEntryImpl::SetReferrer(const Referrer& referrer) {
  frame_tree_->frame_entry->set_referrer(referrer);
}

const Referrer& NavigationEntryImpl::GetReferrer() const {
  return frame_tree_->frame_entry->referrer();
}

void NavigationEntryImpl::SetVirtualURL(const GURL& url) {
  virtual_url_ = (url == GetURL()) ? GURL() : url;
  cached_display_title_.clear();
}

const GURL& NavigationEntryImpl::GetVirtualURL() const {
  return virtual_url_.is_empty() ? GetURL() : virtual_url_;
}

void NavigationEntryImpl::SetTitle(const base::string16& title) {
  title_ = title;
  cached_display_title_.clear();
}

const base::string16& NavigationEntryImpl::GetTitle() const {
  return title_;
}

void NavigationEntryImpl::SetPageState(const PageState& state) {
  // SetPageState should only be called before the NavigationEntry has been
  // loaded, such as for restore (when there are no subframe
  // FrameNavigationEntries yet).  However, some callers expect to call this
  // after a Clone but before loading the page.  Clone will copy over the
  // subframe entries, and we should reset them before setting the state again.
  //
  // TODO(creis): It would be good to verify that this NavigationEntry hasn't
  // been loaded yet in cases that SetPageState is called while subframe
  // entries exist, but there's currently no way to check that.
  if (!frame_tree_->children.empty())
    frame_tree_->children.clear();

  // If the PageState can't be parsed or has no children, just store it on the
  // main frame's FrameNavigationEntry without recursively creating subframe
  // entries.
  ExplodedPageState exploded_state;
  if (!DecodePageState(state.ToEncodedData(), &exploded_state) ||
      exploded_state.top.children.size() == 0U) {
    frame_tree_->frame_entry->SetPageState(state);
    return;
  }

  RecursivelyGenerateFrameEntries(
      exploded_state.top, exploded_state.referenced_files, frame_tree_.get());
}

PageState NavigationEntryImpl::GetPageState() const {
  // Just return the main frame's state if there are no subframe
  // FrameNavigationEntries.
  if (frame_tree_->children.size() == 0U)
    return frame_tree_->frame_entry->page_state();

  // When we're using subframe entries, each FrameNavigationEntry has a
  // frame-specific PageState.  We combine these into an ExplodedPageState tree
  // and generate a full PageState from it.
  ExplodedPageState exploded_state;
  RecursivelyGenerateFrameState(frame_tree_.get(), &exploded_state.top,
                                &exploded_state.referenced_files);

  std::string encoded_data;
  EncodePageState(exploded_state, &encoded_data);
  return PageState::CreateFromEncodedData(encoded_data);
}

void NavigationEntryImpl::set_site_instance(
    scoped_refptr<SiteInstanceImpl> site_instance) {
  // TODO(creis): Update all callers and remove this method.
  frame_tree_->frame_entry->set_site_instance(std::move(site_instance));
}

void NavigationEntryImpl::SetBindings(int bindings) {
  // Ensure this is set to a valid value, and that it stays the same once set.
  CHECK_NE(bindings, kInvalidBindings);
  CHECK(bindings_ == kInvalidBindings || bindings_ == bindings);
  bindings_ = bindings;
}

const base::string16& NavigationEntryImpl::GetTitleForDisplay() const {
  // Most pages have real titles. Don't even bother caching anything if this is
  // the case.
  if (!title_.empty())
    return title_;

  // More complicated cases will use the URLs as the title. This result we will
  // cache since it's more complicated to compute.
  if (!cached_display_title_.empty())
    return cached_display_title_;

  // Use the virtual URL first if any, and fall back on using the real URL.
  base::string16 title;
  if (!virtual_url_.is_empty()) {
    title = url_formatter::FormatUrl(virtual_url_);
  } else if (!GetURL().is_empty()) {
    title = url_formatter::FormatUrl(GetURL());
  }

  // For file:// URLs use the filename as the title, not the full path.
  if (GetURL().SchemeIsFile()) {
    // It is necessary to ignore the reference and query parameters or else
    // looking for slashes might accidentally return one of those values. See
    // https://crbug.com/503003.
    base::string16::size_type refpos = title.find('#');
    base::string16::size_type querypos = title.find('?');
    base::string16::size_type lastpos;
    if (refpos == base::string16::npos)
      lastpos = querypos;
    else if (querypos == base::string16::npos)
      lastpos = refpos;
    else
      lastpos = (refpos < querypos) ? refpos : querypos;
    base::string16::size_type slashpos = title.rfind('/', lastpos);
    if (slashpos != base::string16::npos)
      title = title.substr(slashpos + 1);
  } else if (base::i18n::StringContainsStrongRTLChars(title)) {
    // Wrap the URL in an LTR embedding for proper handling of RTL characters.
    // (RFC 3987 Section 4.1 states that "Bidirectional IRIs MUST be rendered in
    // the same way as they would be if they were in a left-to-right
    // embedding".)
    base::i18n::WrapStringWithLTRFormatting(&title);
  }

#if defined(OS_ANDROID)
  if (GetURL().SchemeIs(url::kContentScheme)) {
    base::string16 file_display_name;
    if (base::MaybeGetFileDisplayName(base::FilePath(GetURL().spec()),
                                      &file_display_name)) {
      title = file_display_name;
    }
  }
#endif

  gfx::ElideString(title, kMaxTitleChars, &cached_display_title_);
  return cached_display_title_;
}

bool NavigationEntryImpl::IsViewSourceMode() const {
  return virtual_url_.SchemeIs(kViewSourceScheme);
}

void NavigationEntryImpl::SetTransitionType(
    ui::PageTransition transition_type) {
  transition_type_ = transition_type;
}

ui::PageTransition NavigationEntryImpl::GetTransitionType() const {
  return transition_type_;
}

const GURL& NavigationEntryImpl::GetUserTypedURL() const {
  return user_typed_url_;
}

void NavigationEntryImpl::SetHasPostData(bool has_post_data) {
  frame_tree_->frame_entry->set_method(has_post_data ? "POST" : "GET");
}

bool NavigationEntryImpl::GetHasPostData() const {
  return frame_tree_->frame_entry->method() == "POST";
}

void NavigationEntryImpl::SetPostID(int64_t post_id) {
  frame_tree_->frame_entry->set_post_id(post_id);
}

int64_t NavigationEntryImpl::GetPostID() const {
  return frame_tree_->frame_entry->post_id();
}

void NavigationEntryImpl::SetPostData(
    const scoped_refptr<network::ResourceRequestBody>& data) {
  post_data_ = static_cast<network::ResourceRequestBody*>(data.get());
}

scoped_refptr<network::ResourceRequestBody> NavigationEntryImpl::GetPostData()
    const {
  return post_data_.get();
}


const FaviconStatus& NavigationEntryImpl::GetFavicon() const {
  return favicon_;
}

FaviconStatus& NavigationEntryImpl::GetFavicon() {
  return favicon_;
}

const SSLStatus& NavigationEntryImpl::GetSSL() const {
  return ssl_;
}

SSLStatus& NavigationEntryImpl::GetSSL() {
  return ssl_;
}

void NavigationEntryImpl::SetOriginalRequestURL(const GURL& original_url) {
  original_request_url_ = original_url;
}

const GURL& NavigationEntryImpl::GetOriginalRequestURL() const {
  return original_request_url_;
}

void NavigationEntryImpl::SetIsOverridingUserAgent(bool override) {
  is_overriding_user_agent_ = override;
}

bool NavigationEntryImpl::GetIsOverridingUserAgent() const {
  return is_overriding_user_agent_;
}

void NavigationEntryImpl::SetTimestamp(base::Time timestamp) {
  timestamp_ = timestamp;
}

base::Time NavigationEntryImpl::GetTimestamp() const {
  return timestamp_;
}

void NavigationEntryImpl::SetHttpStatusCode(int http_status_code) {
  http_status_code_ = http_status_code;
}

int NavigationEntryImpl::GetHttpStatusCode() const {
  return http_status_code_;
}

void NavigationEntryImpl::SetRedirectChain(
    const std::vector<GURL>& redirect_chain) {
  root_node()->frame_entry->set_redirect_chain(redirect_chain);
}

const std::vector<GURL>& NavigationEntryImpl::GetRedirectChain() const {
  return root_node()->frame_entry->redirect_chain();
}

const base::Optional<ReplacedNavigationEntryData>&
NavigationEntryImpl::GetReplacedEntryData() const {
  return replaced_entry_data_;
}

bool NavigationEntryImpl::IsRestored() const {
  return restore_type_ != RestoreType::NONE;
}

std::string NavigationEntryImpl::GetExtraHeaders() const {
  return extra_headers_;
}

void NavigationEntryImpl::AddExtraHeaders(
    const std::string& more_extra_headers) {
  DCHECK(!more_extra_headers.empty());
  if (!extra_headers_.empty())
    extra_headers_ += "\r\n";
  extra_headers_ += more_extra_headers;
}

void NavigationEntryImpl::SetCanLoadLocalResources(bool allow) {
  can_load_local_resources_ = allow;
}

bool NavigationEntryImpl::GetCanLoadLocalResources() const {
  return can_load_local_resources_;
}

void NavigationEntryImpl::SetExtraData(const std::string& key,
                                       const base::string16& data) {
  extra_data_[key] = data;
}

bool NavigationEntryImpl::GetExtraData(const std::string& key,
                                       base::string16* data) const {
  auto iter = extra_data_.find(key);
  if (iter == extra_data_.end())
    return false;
  *data = iter->second;
  return true;
}

void NavigationEntryImpl::ClearExtraData(const std::string& key) {
  extra_data_.erase(key);
}

std::unique_ptr<NavigationEntryImpl> NavigationEntryImpl::Clone() const {
  return NavigationEntryImpl::CloneAndReplace(nullptr, false, nullptr, nullptr);
}

std::unique_ptr<NavigationEntryImpl> NavigationEntryImpl::CloneAndReplace(
    FrameNavigationEntry* frame_navigation_entry,
    bool clone_children_of_target,
    FrameTreeNode* target_frame_tree_node,
    FrameTreeNode* root_frame_tree_node) const {
  std::unique_ptr<NavigationEntryImpl> copy =
      base::WrapUnique(new NavigationEntryImpl());

  // TODO(creis): Only share the same FrameNavigationEntries if cloning within
  // the same tab.
  copy->frame_tree_ = frame_tree_->CloneAndReplace(
      frame_navigation_entry, clone_children_of_target, target_frame_tree_node,
      root_frame_tree_node, nullptr);

  // Copy most state over, unless cleared in ResetForCommit.
  // Don't copy unique_id_, otherwise it won't be unique.
  copy->bindings_ = bindings_;
  copy->page_type_ = page_type_;
  copy->virtual_url_ = virtual_url_;
  copy->update_virtual_url_with_url_ = update_virtual_url_with_url_;
  copy->title_ = title_;
  copy->favicon_ = favicon_;
  copy->ssl_ = ssl_;
  copy->transition_type_ = transition_type_;
  copy->user_typed_url_ = user_typed_url_;
  copy->restore_type_ = restore_type_;
  copy->original_request_url_ = original_request_url_;
  copy->is_overriding_user_agent_ = is_overriding_user_agent_;
  copy->timestamp_ = timestamp_;
  copy->http_status_code_ = http_status_code_;
  // ResetForCommit: post_data_
  copy->screenshot_ = screenshot_;
  copy->extra_headers_ = extra_headers_;
  copy->base_url_for_data_url_ = base_url_for_data_url_;
#if defined(OS_ANDROID)
  copy->data_url_as_string_ = data_url_as_string_;
#endif
  // ResetForCommit: is_renderer_initiated_
  copy->cached_display_title_ = cached_display_title_;
  // ResetForCommit: should_replace_entry_
  // ResetForCommit: should_clear_history_list_
  // ResetForCommit: frame_tree_node_id_
  copy->has_user_gesture_ = has_user_gesture_;
  // ResetForCommit: reload_type_
  copy->extra_data_ = extra_data_;
  copy->replaced_entry_data_ = replaced_entry_data_;

  return copy;
}

CommonNavigationParams NavigationEntryImpl::ConstructCommonNavigationParams(
    const FrameNavigationEntry& frame_entry,
    const scoped_refptr<network::ResourceRequestBody>& post_body,
    const GURL& dest_url,
    const Referrer& dest_referrer,
    FrameMsg_Navigate_Type::Value navigation_type,
    PreviewsState previews_state,
    base::TimeTicks navigation_start,
    base::TimeTicks input_start) const {
  return CommonNavigationParams(
      dest_url, dest_referrer, GetTransitionType(), navigation_type,
      !IsViewSourceMode(), should_replace_entry(), GetBaseURLForDataURL(),
      GetHistoryURLForDataURL(), previews_state, navigation_start,
      frame_entry.method(), post_body ? post_body : post_data_,
      base::Optional<SourceLocation>(), has_started_from_context_menu(),
      has_user_gesture(), InitiatorCSPInfo(), input_start);
}

RequestNavigationParams NavigationEntryImpl::ConstructRequestNavigationParams(
    const FrameNavigationEntry& frame_entry,
    const GURL& original_url,
    const std::string& original_method,
    bool is_history_navigation_in_new_child,
    const std::map<std::string, bool>& subframe_unique_names,
    bool intended_as_new_entry,
    int pending_history_list_offset,
    int current_history_list_offset,
    int current_history_list_length) const {
  // Set the redirect chain to the navigation's redirects, unless returning to a
  // completed navigation (whose previous redirects don't apply).
  std::vector<GURL> redirects;
  if (ui::PageTransitionIsNewNavigation(GetTransitionType())) {
    redirects = frame_entry.redirect_chain();
  }

  int pending_offset_to_send = pending_history_list_offset;
  int current_offset_to_send = current_history_list_offset;
  int current_length_to_send = current_history_list_length;
  if (should_clear_history_list()) {
    // Set the history list related parameters to the same values a
    // NavigationController would return before its first navigation. This will
    // fully clear the RenderView's view of the session history.
    pending_offset_to_send = -1;
    current_offset_to_send = -1;
    current_length_to_send = 0;
  }

  RequestNavigationParams request_params(
      GetIsOverridingUserAgent(), redirects, original_url, original_method,
      GetCanLoadLocalResources(), frame_entry.page_state(), GetUniqueID(),
      is_history_navigation_in_new_child, subframe_unique_names,
      intended_as_new_entry, pending_offset_to_send, current_offset_to_send,
      current_length_to_send, IsViewSourceMode(), should_clear_history_list());
#if defined(OS_ANDROID)
  if (NavigationControllerImpl::ValidateDataURLAsString(GetDataURLAsString())) {
    request_params.data_url_as_string = GetDataURLAsString()->data();
  }
#endif
  return request_params;
}

void NavigationEntryImpl::ResetForCommit(FrameNavigationEntry* frame_entry) {
  // Any state that only matters when a navigation entry is pending should be
  // cleared here.
  // TODO(creis): This state should be moved to NavigationRequest once
  // PlzNavigate is enabled.
  SetPostData(nullptr);
  set_is_renderer_initiated(false);
  set_should_replace_entry(false);

  set_should_clear_history_list(false);
  set_frame_tree_node_id(-1);
  set_reload_type(ReloadType::NONE);

  if (frame_entry) {
    frame_entry->set_source_site_instance(nullptr);
    frame_entry->set_blob_url_loader_factory(nullptr);
  }
}

NavigationEntryImpl::TreeNode* NavigationEntryImpl::GetTreeNode(
    FrameTreeNode* frame_tree_node) const {
  NavigationEntryImpl::TreeNode* node = nullptr;
  base::queue<NavigationEntryImpl::TreeNode*> work_queue;
  work_queue.push(root_node());
  while (!work_queue.empty()) {
    node = work_queue.front();
    work_queue.pop();
    if (node->MatchesFrame(frame_tree_node))
      return node;

    // Enqueue any children and keep looking.
    for (const auto& child : node->children)
      work_queue.push(child.get());
  }
  return nullptr;
}

void NavigationEntryImpl::AddOrUpdateFrameEntry(
    FrameTreeNode* frame_tree_node,
    int64_t item_sequence_number,
    int64_t document_sequence_number,
    SiteInstanceImpl* site_instance,
    scoped_refptr<SiteInstanceImpl> source_site_instance,
    const GURL& url,
    const Referrer& referrer,
    const std::vector<GURL>& redirect_chain,
    const PageState& page_state,
    const std::string& method,
    int64_t post_id,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory) {
  // If this is called for the main frame, the FrameNavigationEntry is
  // guaranteed to exist, so just update it directly and return.
  if (frame_tree_node->IsMainFrame()) {
    // If the document of the FrameNavigationEntry is changing, we must clear
    // any child FrameNavigationEntries.
    if (root_node()->frame_entry->document_sequence_number() !=
        document_sequence_number)
      root_node()->children.clear();

    root_node()->frame_entry->UpdateEntry(
        frame_tree_node->unique_name(), item_sequence_number,
        document_sequence_number, site_instance,
        std::move(source_site_instance), url, referrer, redirect_chain,
        page_state, method, post_id, std::move(blob_url_loader_factory));
    return;
  }

  // We should already have a TreeNode for the parent node by the time this node
  // commits.  Find it first.
  NavigationEntryImpl::TreeNode* parent_node =
      GetTreeNode(frame_tree_node->parent());
  if (!parent_node) {
    // The renderer should not send a commit for a subframe before its parent.
    // TODO(creis): Kill the renderer if we get here.
    return;
  }

  // Now check whether we have a TreeNode for the node itself.
  const std::string& unique_name = frame_tree_node->unique_name();
  for (const auto& child : parent_node->children) {
    if (child->frame_entry->frame_unique_name() == unique_name) {
      // If the document of the FrameNavigationEntry is changing, we must clear
      // any child FrameNavigationEntries.
      if (child->frame_entry->document_sequence_number() !=
          document_sequence_number)
        child->children.clear();

      // Update the existing FrameNavigationEntry (e.g., for replaceState).
      child->frame_entry->UpdateEntry(
          unique_name, item_sequence_number, document_sequence_number,
          site_instance, std::move(source_site_instance), url, referrer,
          redirect_chain, page_state, method, post_id,
          std::move(blob_url_loader_factory));
      return;
    }
  }

  // No entry exists yet, so create a new one.
  // Unordered list, since we expect to look up entries by frame sequence number
  // or unique name.
  FrameNavigationEntry* frame_entry = new FrameNavigationEntry(
      unique_name, item_sequence_number, document_sequence_number,
      site_instance, std::move(source_site_instance), url, referrer,
      redirect_chain, page_state, method, post_id,
      std::move(blob_url_loader_factory));
  parent_node->children.push_back(
      std::make_unique<NavigationEntryImpl::TreeNode>(parent_node,
                                                      frame_entry));
}

FrameNavigationEntry* NavigationEntryImpl::GetFrameEntry(
    FrameTreeNode* frame_tree_node) const {
  NavigationEntryImpl::TreeNode* tree_node = GetTreeNode(frame_tree_node);
  return tree_node ? tree_node->frame_entry.get() : nullptr;
}

std::map<std::string, bool> NavigationEntryImpl::GetSubframeUniqueNames(
    FrameTreeNode* frame_tree_node) const {
  std::map<std::string, bool> names;
  NavigationEntryImpl::TreeNode* tree_node = GetTreeNode(frame_tree_node);
  if (tree_node) {
    // Return the names of all immediate children.
    for (const auto& child : tree_node->children) {
      // Keep track of whether we would be loading about:blank, since the
      // renderer should be allowed to just commit the initial blank frame if
      // that was the default URL.  PageState doesn't matter there, because
      // content injected into about:blank frames doesn't use it.
      //
      // Be careful not to rely on FrameNavigationEntry's URLs in this check,
      // because the committed URL in the browser could be rewritten to
      // about:blank.
      // See RenderProcessHostImpl::FilterURL to know which URLs are rewritten.
      // See https://crbug.com/657896 for details.
      bool is_about_blank = false;
      ExplodedPageState exploded_page_state;
      if (DecodePageState(child->frame_entry->page_state().ToEncodedData(),
                          &exploded_page_state)) {
        ExplodedFrameState frame_state = exploded_page_state.top;
        if (UTF16ToUTF8(frame_state.url_string.value_or(base::string16())) ==
            url::kAboutBlankURL)
          is_about_blank = true;
      }

      names[child->frame_entry->frame_unique_name()] = is_about_blank;
    }
  }
  return names;
}

void NavigationEntryImpl::RemoveEntryForFrame(FrameTreeNode* frame_tree_node,
                                              bool only_if_different_position) {
  DCHECK(!frame_tree_node->IsMainFrame());

  NavigationEntryImpl::TreeNode* node = GetTreeNode(frame_tree_node);
  if (!node)
    return;

  // Remove the |node| from the tree if either 1) |only_if_different_position|
  // was not asked for or 2) if it is not in the same position in the tree of
  // FrameNavigationEntries and the FrameTree.
  if (!only_if_different_position ||
      !InSameTreePosition(frame_tree_node, node)) {
    NavigationEntryImpl::TreeNode* parent_node = node->parent;
    auto it = std::find_if(
        parent_node->children.begin(), parent_node->children.end(),
        [node](const std::unique_ptr<NavigationEntryImpl::TreeNode>& item) {
          return item.get() == node;
        });
    CHECK(it != parent_node->children.end());
    parent_node->children.erase(it);
  }
}

void NavigationEntryImpl::SetScreenshotPNGData(
    scoped_refptr<base::RefCountedBytes> png_data) {
  screenshot_ = png_data;
  if (screenshot_.get())
    UMA_HISTOGRAM_MEMORY_KB("Overscroll.ScreenshotSize", screenshot_->size());
}

GURL NavigationEntryImpl::GetHistoryURLForDataURL() const {
  return GetBaseURLForDataURL().is_empty() ? GURL() : GetVirtualURL();
}

}  // namespace content
