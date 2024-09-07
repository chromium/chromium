// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_entry_impl.h"

#include <stddef.h>

#include <map>
#include <string>
#include <utility>

#include "base/containers/queue.h"
#include "base/files/file_path.h"
#include "base/i18n/rtl.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/url_formatter/url_formatter.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_entry_restore_context_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/common/content_constants_internal.h"
#include "content/public/browser/reload_type.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/url_constants.h"
#include "net/storage_access_api/status.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/common/navigation/navigation_params.h"
#include "third_party/blink/public/common/page_state/page_state_serialization.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom.h"
#include "third_party/blink/public/mojom/navigation/prefetched_signed_exchange_info.mojom.h"
#include "third_party/blink/public/mojom/runtime_feature_state/runtime_feature.mojom.h"
#include "ui/gfx/text_elider.h"

#if BUILDFLAG(IS_ANDROID)
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
    NavigationEntryRestoreContextImpl* context,
    const blink::ExplodedFrameState& state,
    const std::vector<std::optional<std::u16string>>& referenced_files,
    NavigationEntryImpl::TreeNode* node) {
  DCHECK(context);
  // Set a single-frame PageState on the entry.
  blink::ExplodedPageState page_state;

  // Copy the incoming PageState's list of referenced files into the main
  // frame's PageState.  (We don't pass the list to subframes below.)
  // TODO(creis): Grant access to this list for each process that renders
  // this page, even for OOPIFs.  Eventually keep track of a verified list of
  // files per frame, so that we only grant access to processes that need it.
  if (referenced_files.size() > 0)
    page_state.referenced_files = referenced_files;

  page_state.top = state;
  std::string data;
  blink::EncodePageState(page_state, &data);
  DCHECK(!data.empty()) << "Shouldn't generate an empty PageState.";

  // Attempt to find an existing FrameNavigationEntry from `context`, only if it
  // matches by ISN, URL, and target. Note that it is possible for other values
  // to still diverge between `entry` and `state` when those match (see
  // https://crbug.com/1354634), but these values are considered sufficient to
  // treat the FrameNavigationEntry as shared in future sessions.
  GURL state_url(state.url_string.value_or(std::u16string()));
  scoped_refptr<FrameNavigationEntry> entry = context->GetFrameNavigationEntry(
      state.item_sequence_number,
      state.target ? base::UTF16ToUTF8(*state.target) : "", state_url);

  if (!entry) {
    std::optional<GURL> initiator_base_url;
    if (state.initiator_base_url_string) {
      GURL initiator_base_url_from_state =
          GURL(UTF16ToUTF8(state.initiator_base_url_string.value()));
      // If `state.initiator_base_url_string` has a value, it should be
      // non-empty. But it's easiest and safest to just check, as opposed to
      // using a CHECK.
      if (!initiator_base_url_from_state.is_empty()) {
        initiator_base_url = initiator_base_url_from_state;
      }
    }
    entry = base::MakeRefCounted<FrameNavigationEntry>(
        UTF16ToUTF8(state.target.value_or(std::u16string())),
        state.item_sequence_number, state.document_sequence_number,
        UTF16ToUTF8(state.navigation_api_key.value_or(std::u16string())),
        nullptr, nullptr, state_url,
        // TODO(nasko): Supply valid origin once the value is persisted across
        // session restore.
        std::nullopt /* origin */,
        Referrer(GURL(state.referrer.value_or(std::u16string())),
                 state.referrer_policy),
        state.initiator_origin, initiator_base_url, std::vector<GURL>(),
        blink::PageState::CreateFromEncodedData(data), "GET", -1,
        nullptr /* blob_url_loader_factory */,
        // TODO(crbug.com/40053667): We should restore the policy
        // container.
        nullptr /* policy_container_policies */,
        state.protect_url_in_navigation_api);
    context->AddFrameNavigationEntry(entry.get());
  }
  node->frame_entry = std::move(entry);

  // Don't pass the file list to subframes, since that would result in multiple
  // copies of it ending up in the combined list in GetPageState (via
  // RecursivelyGenerateFrameState).
  std::vector<std::optional<std::u16string>> empty_file_list;

  for (const blink::ExplodedFrameState& child_state : state.children) {
    node->children.push_back(
        std::make_unique<NavigationEntryImpl::TreeNode>(node, nullptr));
    RecursivelyGenerateFrameEntries(context, child_state, empty_file_list,
                                    node->children.back().get());
  }
}

std::optional<std::u16string> UrlToOptionalString16(const GURL& url) {
  if (!url.is_valid())
    return std::nullopt;
  return base::UTF8ToUTF16(url.spec());
}

void RecursivelyGenerateFrameState(
    NavigationEntryImpl::TreeNode* node,
    blink::ExplodedFrameState* state,
    std::vector<std::optional<std::u16string>>* referenced_files) {
  // The FrameNavigationEntry's PageState contains just the ExplodedFrameState
  // for that particular frame.
  blink::ExplodedPageState exploded_page_state;
  if (!blink::DecodePageState(node->frame_entry->page_state().ToEncodedData(),
                              &exploded_page_state)) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  blink::ExplodedFrameState frame_state = exploded_page_state.top;

  // Copy the FrameNavigationEntry's frame state into the destination state.
  *state = frame_state;

  // Some data is stored *both* in 1) PageState/ExplodedFrameState and 2)
  // FrameNavigationEntry.  We want to treat FrameNavigationEntry as the
  // authoritative source of the data, so we clobber the ExplodedFrameState with
  // the data taken from FrameNavigationEntry.
  //
  // The following ExplodedFrameState fields do not have an equivalent
  // FrameNavigationEntry field:
  // - target
  // - state_object
  // - document_state
  // - scroll_restoration_type
  // - did_save_scroll_or_scale_state
  // - visual_viewport_scroll_offset
  // - scroll_offset
  // - page_scale_factor
  // - http_body (FrameNavigationEntry::GetPostData extracts the body from
  //   the ExplodedFrameState)
  // - scroll_anchor_selector
  // - scroll_anchor_offset
  // - scroll_anchor_simhash
  state->url_string = UrlToOptionalString16(node->frame_entry->url());
  state->referrer = UrlToOptionalString16(node->frame_entry->referrer().url);
  state->referrer_policy = node->frame_entry->referrer().policy;
  state->item_sequence_number = node->frame_entry->item_sequence_number();
  state->document_sequence_number =
      node->frame_entry->document_sequence_number();
  state->initiator_origin = node->frame_entry->initiator_origin();
  // Note: If UrlToOptionalString16 receives an empty GURL, it returns nullopt.
  state->initiator_base_url_string = UrlToOptionalString16(
      node->frame_entry->initiator_base_url().value_or(GURL()));

  // protect_url_in_navigation_api is not generated by the renderer, it is only
  // stored on FrameNavigationEntry.
  state->protect_url_in_navigation_api =
      node->frame_entry->protect_url_in_navigation_api();

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
  FrameTreeNode* ftn = FrameTreeNode::From(frame_tree_node->parent());
  NavigationEntryImpl::TreeNode* current_node = node->parent;
  while (ftn && current_node) {
    if (!current_node->MatchesFrame(ftn))
      return false;

    if ((!current_node->parent && ftn->parent()) ||
        (current_node->parent && !ftn->parent())) {
      return false;
    }

    ftn = FrameTreeNode::From(ftn->parent());
    current_node = current_node->parent;
  }
  return true;
}

void RegisterOriginsRecursive(NavigationEntryImpl::TreeNode* node,
                              const url::Origin& origin) {
  if (node->frame_entry->committed_origin().has_value()) {
    const url::Origin node_origin =
        node->frame_entry->committed_origin().value();
    SiteInstanceImpl* site_instance = node->frame_entry->site_instance();
    if (site_instance && origin == node_origin)
      site_instance->RegisterAsDefaultOriginIsolation(node_origin);
  }

  for (auto& child : node->children)
    RegisterOriginsRecursive(child.get(), origin);
}

}  // namespace

void NavigationEntryImpl::RegisterExistingOriginAsHavingDefaultIsolation(
    const url::Origin& origin) {
  return RegisterOriginsRecursive(root_node(), origin);
}

NavigationEntryImpl::TreeNode::TreeNode(
    TreeNode* parent,
    scoped_refptr<FrameNavigationEntry> frame_entry)
    : parent(parent), frame_entry(std::move(frame_entry)) {}

NavigationEntryImpl::TreeNode::~TreeNode() {}

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
    scoped_refptr<FrameNavigationEntry> frame_navigation_entry,
    bool clone_children_of_target,
    FrameTreeNode* target_frame_tree_node,
    FrameTreeNode* current_frame_tree_node,
    TreeNode* parent_node,
    NavigationEntryRestoreContextImpl* restore_context,
    ClonePolicy clone_policy) const {
  // |restore_context| should only ever be used when doing a deep clone, and
  // when there is no target.
  if (restore_context) {
    DCHECK(!frame_navigation_entry && !target_frame_tree_node &&
           clone_policy == ClonePolicy::kCloneFrameEntries);
  }

  // Clone this TreeNode, possibly replacing its FrameNavigationEntry.
  bool is_target_frame =
      target_frame_tree_node && MatchesFrame(target_frame_tree_node);

  scoped_refptr<FrameNavigationEntry> new_entry;
  if (is_target_frame) {
    new_entry = frame_navigation_entry;
  } else if (clone_policy == ClonePolicy::kShareFrameEntries) {
    new_entry = frame_entry;
  } else {
    if (restore_context) {
      // If |restore_context| is given and already has a FrameNavigationEntry
      // for the given item sequence number and URL, share that
      // FrameNavigationEntry rather than creating a duplicate.
      new_entry = restore_context->GetFrameNavigationEntry(
          frame_entry->item_sequence_number(), frame_entry->frame_unique_name(),
          frame_entry->url());
    }
    if (!new_entry) {
      new_entry = frame_entry->Clone();
      if (restore_context)
        restore_context->AddFrameNavigationEntry(new_entry.get());
    }
  }

  auto copy = std::make_unique<NavigationEntryImpl::TreeNode>(
      parent_node, std::move(new_entry));

  // Recursively clone the children if needed.
  if (!is_target_frame || clone_children_of_target) {
    for (size_t i = 0; i < children.size(); i++) {
      const auto& child = children[i];

      // Don't check whether it's still in the tree if |current_frame_tree_node|
      // is null.
      if (!current_frame_tree_node) {
        copy->children.push_back(child->CloneAndReplace(
            frame_navigation_entry, clone_children_of_target,
            target_frame_tree_node, nullptr, copy.get(), restore_context,
            clone_policy));
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
              copy.get(), restore_context, clone_policy));
          break;
        }
      }
    }
  }

  return copy;
}

std::unique_ptr<NavigationEntry> NavigationEntry::Create() {
  return std::make_unique<NavigationEntryImpl>();
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
                          /* initiator_origin= */ std::nullopt,
                          /* initiator_base_url= */ std::nullopt,
                          std::u16string(),
                          ui::PAGE_TRANSITION_LINK,
                          false,
                          nullptr,
                          /* is_initial_entry = */ false) {}

NavigationEntryImpl::NavigationEntryImpl(
    scoped_refptr<SiteInstanceImpl> instance,
    const GURL& url,
    const Referrer& referrer,
    const std::optional<url::Origin>& initiator_origin,
    const std::optional<GURL>& initiator_base_url,
    const std::u16string& title,
    ui::PageTransition transition_type,
    bool is_renderer_initiated,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
    bool is_initial_entry)
    : frame_tree_(std::make_unique<TreeNode>(
          nullptr,
          base::MakeRefCounted<FrameNavigationEntry>(
              "",
              -1,
              -1,
              "",
              std::move(instance),
              nullptr,
              url,
              std::nullopt /* origin */,
              referrer,
              initiator_origin,
              initiator_base_url,
              std::vector<GURL>(),
              blink::PageState(),
              "GET",
              -1,
              std::move(blob_url_loader_factory),
              nullptr /* policy_container_policies */,
              false /* protect_url_in_navigation_api */))),
      unique_id_(CreateUniqueEntryID()),
      page_type_(PAGE_TYPE_NORMAL),
      update_virtual_url_with_url_(false),
      title_(title),
      transition_type_(transition_type),
      restore_type_(RestoreType::kNotRestored),
      is_overriding_user_agent_(false),
      http_status_code_(0),
      is_renderer_initiated_(is_renderer_initiated),
      should_clear_history_list_(false),
      can_load_local_resources_(false),
      has_user_gesture_(false),
      reload_type_(ReloadType::NONE),
      started_from_context_menu_(false),
      ssl_error_(false),
      should_skip_on_back_forward_ui_(false),
      initial_navigation_entry_state_(
          is_initial_entry
              ? InitialNavigationEntryState::kInitialNotForSynchronousAboutBlank
              : InitialNavigationEntryState::kNonInitial) {}

NavigationEntryImpl::~NavigationEntryImpl() {
  if (navigation_transition_data_
          .same_document_navigation_entry_screenshot_token()
          .has_value()) {
    // We get here if:
    // - `DidCommitSameDocumentNavigation` sets the token, promising a
    //    screenshot was supposed to arrive.
    // - However the navigation entry was destroyed before the screenshot could
    //   arrive.
    viz::HostFrameSinkManager* manager = GetHostFrameSinkManager();
    CHECK(manager);
    manager->InvalidateCopyOutputReadyCallback(
        navigation_transition_data_
            .same_document_navigation_entry_screenshot_token()
            .value());
  }
}

int NavigationEntryImpl::GetUniqueID() {
  return unique_id_;
}

PageType NavigationEntryImpl::GetPageType() {
  return page_type_;
}

void NavigationEntryImpl::SetURL(const GURL& url) {
  frame_tree_->frame_entry->set_url(url);
  cached_display_title_.clear();
}

const GURL& NavigationEntryImpl::GetURL() {
  return frame_tree_->frame_entry->url();
}

void NavigationEntryImpl::SetBaseURLForDataURL(const GURL& url) {
  base_url_for_data_url_ = url;
}

const GURL& NavigationEntryImpl::GetBaseURLForDataURL() {
  return base_url_for_data_url_;
}

#if BUILDFLAG(IS_ANDROID)
void NavigationEntryImpl::SetDataURLAsString(
    scoped_refptr<base::RefCountedString> data_url) {
  if (data_url) {
    // A quick check that it's actually a data URL.
    DCHECK(base::StartsWith(base::as_string_view(*data_url), url::kDataScheme,
                            base::CompareCase::SENSITIVE));
  }
  data_url_as_string_ = std::move(data_url);
}

const scoped_refptr<const base::RefCountedString>&
NavigationEntryImpl::GetDataURLAsString() {
  return data_url_as_string_;
}
#endif

void NavigationEntryImpl::SetReferrer(const Referrer& referrer) {
  frame_tree_->frame_entry->set_referrer(referrer);
}

const Referrer& NavigationEntryImpl::GetReferrer() {
  return frame_tree_->frame_entry->referrer();
}

void NavigationEntryImpl::SetVirtualURL(const GURL& url) {
  virtual_url_ = (url == GetURL()) ? GURL() : url;
  cached_display_title_.clear();
}

const GURL& NavigationEntryImpl::GetVirtualURL() {
  return virtual_url_.is_empty() ? GetURL() : virtual_url_;
}

void NavigationEntryImpl::SetTitle(std::u16string title) {
  title_ = std::move(title);
  cached_display_title_.clear();
}

const std::u16string& NavigationEntryImpl::GetTitle() {
  return title_;
}

void NavigationEntryImpl::SetAppTitle(const std::u16string& app_title) {
  app_title_ = app_title;
}

const std::optional<std::u16string>& NavigationEntryImpl::GetAppTitle() {
  return app_title_;
}

void NavigationEntryImpl::SetPageState(const blink::PageState& state,
                                       NavigationEntryRestoreContext* context) {
  DCHECK(state.IsValid());
  DCHECK(context);

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

  // If the PageState can't be parsed, store a clean PageState for the URL
  // without recursively creating subframe entries. This ensures that the
  // renderer and future sessions will be able to handle the history item, even
  // if not all data can be preserved. See https://crbug.com/1196330.
  blink::ExplodedPageState exploded_state;
  if (!blink::DecodePageState(state.ToEncodedData(), &exploded_state)) {
    // Replace frame_entry with a clone to avoid sharing with any other
    // NavigationEntries, because the item sequence number will be gone.
    frame_tree_->frame_entry = frame_tree_->frame_entry->Clone();
    frame_tree_->frame_entry->SetPageState(
        blink::PageState::CreateFromURL(GetURL()));
    return;
  }

  RecursivelyGenerateFrameEntries(
      static_cast<NavigationEntryRestoreContextImpl*>(context),
      exploded_state.top, exploded_state.referenced_files, frame_tree_.get());
}

blink::PageState NavigationEntryImpl::GetPageState() {
  // Each FrameNavigationEntry has a frame-specific PageState.  We combine these
  // into an ExplodedPageState tree and generate a full PageState from it.
  blink::ExplodedPageState exploded_state;
  RecursivelyGenerateFrameState(frame_tree_.get(), &exploded_state.top,
                                &exploded_state.referenced_files);

  std::string encoded_data;
  blink::EncodePageState(exploded_state, &encoded_data);
  return blink::PageState::CreateFromEncodedData(encoded_data);
}

const std::u16string& NavigationEntryImpl::GetTitleForDisplay() {
  // Most pages have real titles. Don't even bother caching anything if this is
  // the case.
  if (!title_.empty())
    return title_;

  // More complicated cases will use the URLs as the title. This result we will
  // cache since it's more complicated to compute.
  if (!cached_display_title_.empty())
    return cached_display_title_;

  // Use the virtual URL first if any, and fall back on using the real URL.
  std::u16string title;
  if (!virtual_url_.is_empty() || !GetURL().is_empty()) {
    title = url_formatter::FormatUrl(
        virtual_url_.is_empty() ? GetURL() : virtual_url_,
        url_formatter::kFormatUrlOmitDefaults |
            url_formatter::kFormatUrlOmitTrivialSubdomains |
            url_formatter::kFormatUrlOmitHTTPS,
        base::UnescapeRule::SPACES, nullptr, nullptr, nullptr);
  }

  // For file:// URLs use the filename as the title, not the full path.
  if (GetURL().SchemeIsFile()) {
    // It is necessary to ignore the reference and query parameters or else
    // looking for slashes might accidentally return one of those values. See
    // https://crbug.com/503003.
    std::u16string::size_type refpos = title.find('#');
    std::u16string::size_type querypos = title.find('?');
    std::u16string::size_type lastpos;
    if (refpos == std::u16string::npos)
      lastpos = querypos;
    else if (querypos == std::u16string::npos)
      lastpos = refpos;
    else
      lastpos = (refpos < querypos) ? refpos : querypos;
    std::u16string::size_type slashpos = title.rfind('/', lastpos);
    if (slashpos != std::u16string::npos)
      title = title.substr(slashpos + 1);

  } else if (GetURL().SchemeIs(kChromeUIUntrustedScheme)) {
    // For chrome-untrusted:// URLs, leave title blank until the page loads.
    title = std::u16string();

  } else if (base::i18n::StringContainsStrongRTLChars(title)) {
    // Wrap the URL in an LTR embedding for proper handling of RTL characters.
    // (RFC 3987 Section 4.1 states that "Bidirectional IRIs MUST be rendered in
    // the same way as they would be if they were in a left-to-right
    // embedding".)
    base::i18n::WrapStringWithLTRFormatting(&title);
  }

#if BUILDFLAG(IS_ANDROID)
  if (GetURL().SchemeIs(url::kContentScheme)) {
    std::u16string file_display_name;
    if (base::MaybeGetFileDisplayName(base::FilePath(GetURL().spec()),
                                      &file_display_name)) {
      title = file_display_name;
    }
  }
#endif

  gfx::ElideString(title, blink::mojom::kMaxTitleChars, &cached_display_title_);
  return cached_display_title_;
}

bool NavigationEntryImpl::IsViewSourceMode() {
  return virtual_url_.SchemeIs(kViewSourceScheme);
}

void NavigationEntryImpl::SetTransitionType(
    ui::PageTransition transition_type) {
  transition_type_ = transition_type;
}

ui::PageTransition NavigationEntryImpl::GetTransitionType() {
  return transition_type_;
}

const GURL& NavigationEntryImpl::GetUserTypedURL() {
  return user_typed_url_;
}

void NavigationEntryImpl::SetHasPostData(bool has_post_data) {
  frame_tree_->frame_entry->set_method(has_post_data ? "POST" : "GET");
}

bool NavigationEntryImpl::GetHasPostData() {
  return frame_tree_->frame_entry->method() == "POST";
}

void NavigationEntryImpl::SetPostID(int64_t post_id) {
  frame_tree_->frame_entry->set_post_id(post_id);
}

int64_t NavigationEntryImpl::GetPostID() {
  return frame_tree_->frame_entry->post_id();
}

void NavigationEntryImpl::SetPostData(
    const scoped_refptr<network::ResourceRequestBody>& data) {
  post_data_ = static_cast<network::ResourceRequestBody*>(data.get());
}

scoped_refptr<network::ResourceRequestBody> NavigationEntryImpl::GetPostData() {
  return post_data_.get();
}

FaviconStatus& NavigationEntryImpl::GetFavicon() {
  return favicon_;
}

SSLStatus& NavigationEntryImpl::GetSSL() {
  return ssl_;
}

void NavigationEntryImpl::SetOriginalRequestURL(const GURL& original_url) {
  original_request_url_ = original_url;
}

const GURL& NavigationEntryImpl::GetOriginalRequestURL() {
  return original_request_url_;
}

void NavigationEntryImpl::SetIsOverridingUserAgent(bool override_ua) {
  is_overriding_user_agent_ = override_ua;
}

bool NavigationEntryImpl::GetIsOverridingUserAgent() {
  return is_overriding_user_agent_;
}

void NavigationEntryImpl::SetTimestamp(base::Time timestamp) {
  timestamp_ = timestamp;
}

base::Time NavigationEntryImpl::GetTimestamp() {
  return timestamp_;
}

void NavigationEntryImpl::SetHttpStatusCode(int http_status_code) {
  http_status_code_ = http_status_code;
}

int NavigationEntryImpl::GetHttpStatusCode() {
  return http_status_code_;
}

void NavigationEntryImpl::SetRedirectChain(
    const std::vector<GURL>& redirect_chain) {
  root_node()->frame_entry->set_redirect_chain(redirect_chain);
}

const std::vector<GURL>& NavigationEntryImpl::GetRedirectChain() {
  return root_node()->frame_entry->redirect_chain();
}

const std::optional<ReplacedNavigationEntryData>&
NavigationEntryImpl::GetReplacedEntryData() {
  return replaced_entry_data_;
}

bool NavigationEntryImpl::IsRestored() {
  return restore_type_ == RestoreType::kRestored;
}

std::string NavigationEntryImpl::GetExtraHeaders() {
  return extra_headers_;
}

void NavigationEntryImpl::AddExtraHeaders(
    const std::string& more_extra_headers) {
  DCHECK(!more_extra_headers.empty());
  if (!extra_headers_.empty())
    extra_headers_ += "\r\n";
  extra_headers_ += more_extra_headers;
}

int64_t NavigationEntryImpl::GetMainFrameDocumentSequenceNumber() {
  return frame_tree_->frame_entry->document_sequence_number();
}

void NavigationEntryImpl::SetCanLoadLocalResources(bool allow) {
  can_load_local_resources_ = allow;
}

bool NavigationEntryImpl::GetCanLoadLocalResources() {
  return can_load_local_resources_;
}

bool NavigationEntryImpl::IsInitialEntry() {
  return initial_navigation_entry_state_ !=
         InitialNavigationEntryState::kNonInitial;
}

std::unique_ptr<NavigationEntryImpl> NavigationEntryImpl::Clone() const {
  std::unique_ptr<NavigationEntryImpl> entry =
      CloneAndReplaceInternal(nullptr, false, nullptr, nullptr, nullptr,
                              ClonePolicy::kShareFrameEntries);
  // This function is only used for creating pending entries, which should not
  // carry the "initial" status.
  entry->set_initial_navigation_entry_state(
      InitialNavigationEntryState::kNonInitial);
  return entry;
}

std::unique_ptr<NavigationEntryImpl> NavigationEntryImpl::CloneWithoutSharing(
    NavigationEntryRestoreContextImpl* restore_context) const {
  DCHECK(restore_context);
  return CloneAndReplaceInternal(nullptr, false, nullptr, nullptr,
                                 restore_context,
                                 ClonePolicy::kCloneFrameEntries);
}

std::unique_ptr<NavigationEntryImpl> NavigationEntryImpl::CloneAndReplace(
    scoped_refptr<FrameNavigationEntry> frame_navigation_entry,
    bool clone_children_of_target,
    FrameTreeNode* target_frame_tree_node,
    FrameTreeNode* root_frame_tree_node) const {
  std::unique_ptr<NavigationEntryImpl> entry = CloneAndReplaceInternal(
      frame_navigation_entry, clone_children_of_target, target_frame_tree_node,
      root_frame_tree_node, nullptr, ClonePolicy::kShareFrameEntries);
  return entry;
}

std::unique_ptr<NavigationEntryImpl>
NavigationEntryImpl::CloneAndReplaceInternal(
    scoped_refptr<FrameNavigationEntry> frame_navigation_entry,
    bool clone_children_of_target,
    FrameTreeNode* target_frame_tree_node,
    FrameTreeNode* root_frame_tree_node,
    NavigationEntryRestoreContextImpl* restore_context,
    ClonePolicy clone_policy) const {
  auto copy = std::make_unique<NavigationEntryImpl>();

  copy->frame_tree_ = frame_tree_->CloneAndReplace(
      std::move(frame_navigation_entry), clone_children_of_target,
      target_frame_tree_node, root_frame_tree_node, nullptr, restore_context,
      clone_policy);

  // Copy most state over, unless cleared in ResetForCommit.
  // Don't copy unique_id_, otherwise it won't be unique.
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
  copy->extra_headers_ = extra_headers_;
  copy->base_url_for_data_url_ = base_url_for_data_url_;
#if BUILDFLAG(IS_ANDROID)
  copy->data_url_as_string_ = data_url_as_string_;
#endif
  // ResetForCommit: is_renderer_initiated_
  copy->cached_display_title_ = cached_display_title_;
  // ResetForCommit: should_replace_entry_
  // ResetForCommit: should_clear_history_list_
  // ResetForCommit: frame_tree_node_id_
  copy->has_user_gesture_ = has_user_gesture_;
  // ResetForCommit: reload_type_
  copy->CloneDataFrom(*this);
  copy->replaced_entry_data_ = replaced_entry_data_;
  copy->should_skip_on_back_forward_ui_ = should_skip_on_back_forward_ui_;
  copy->initial_navigation_entry_state_ = initial_navigation_entry_state_;

  if (navigation_transition_data().cache_hit_or_miss_reason() ==
      NavigationTransitionData::CacheHitOrMissReason::kCacheHit) {
    copy->navigation_transition_data().set_cache_hit_or_miss_reason(
        NavigationTransitionData::CacheHitOrMissReason::
            kCacheMissClonedNavigationEntry);
  } else {
    copy->navigation_transition_data().set_cache_hit_or_miss_reason(
        navigation_transition_data().cache_hit_or_miss_reason());
  }

  return copy;
}

blink::mojom::CommonNavigationParamsPtr
NavigationEntryImpl::ConstructCommonNavigationParams(
    const FrameNavigationEntry& frame_entry,
    const scoped_refptr<network::ResourceRequestBody>& post_body,
    const GURL& dest_url,
    blink::mojom::ReferrerPtr dest_referrer,
    blink::mojom::NavigationType navigation_type,
    base::TimeTicks navigation_start,
    base::TimeTicks input_start) {
  // `base_url_for_data_url` is saved in NavigationEntry but should only be used
  // by main frames, because loadData* navigations can only happen on the main
  // frame.
  bool is_for_main_frame = (root_node()->frame_entry == &frame_entry);
  // Even if the frame_entry was originally about:blank or about:srcdoc and had
  // an initiator_base_url, there's no guarantee here that `dest_url` will be
  // either about:blank or about:srcdoc. In that case make sure we don't
  // propagate initiator_base_url.
  // TODO(creis): Look into how this case can be avoided so that dest_url
  // doesn't diverge from other parameters in frame_entry.
  const std::optional<GURL> initiator_base_url =
      (dest_url.IsAboutBlank() || dest_url.IsAboutSrcdoc())
          ? frame_entry.initiator_base_url()
          : std::nullopt;
  return blink::mojom::CommonNavigationParams::New(
      dest_url, frame_entry.initiator_origin(), initiator_base_url,
      std::move(dest_referrer), GetTransitionType(), navigation_type,
      blink::NavigationDownloadPolicy(),
      // It's okay to pass false for `should_replace_entry` because we never
      // replace an entry on session history / reload / restore navigation. New
      // navigation that may use replacement create their CommonNavigationParams
      // via NavigationRequest, for example, instead of via NavigationEntry.
      false /* should_replace_entry */,
      is_for_main_frame ? GetBaseURLForDataURL() : GURL(), navigation_start,
      frame_entry.method(), post_body ? post_body : post_data_,
      network::mojom::SourceLocation::New(), has_started_from_context_menu(),
      has_user_gesture(), false /* has_text_fragment_token */,
      network::mojom::CSPDisposition::CHECK, std::vector<int>(), std::string(),
      false /* is_history_navigation_in_new_child_frame */, input_start,
      network::mojom::RequestDestination::kEmpty);
}

blink::mojom::CommitNavigationParamsPtr
NavigationEntryImpl::ConstructCommitNavigationParams(
    const FrameNavigationEntry& frame_entry,
    const GURL& original_url,
    const std::string& original_method,
    const base::flat_map<std::string, bool>& subframe_unique_names,
    bool intended_as_new_entry,
    int pending_history_list_offset,
    int current_history_list_offset,
    int current_history_list_length,
    const blink::FramePolicy& frame_policy,
    bool ancestor_or_self_has_cspee,
    blink::mojom::SystemEntropy system_entropy_at_navigation_start,
    std::optional<blink::scheduler::TaskAttributionId>
        soft_navigation_heuristics_task_id) {
  // Set the redirect chain to the navigation's redirects, unless returning to a
  // completed navigation (whose previous redirects don't apply).
  // Note that this is actually does not work as intended right now because
  // we're only copying the redirect URLs into the new CommitNavigationParams,
  // keeping redirect_response and redirect_infos as empty.
  // TODO(crbug.com/40166073): Save redirect_response & redirect_infos in
  // FNE and copy them too?
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

  blink::mojom::CommitNavigationParamsPtr commit_params =
      blink::mojom::CommitNavigationParams::New(
          std::nullopt,
          // The correct storage key will be computed before committing the
          // navigation.
          blink::StorageKey(), GetIsOverridingUserAgent(), redirects,
          std::vector<network::mojom::URLResponseHeadPtr>(),
          std::vector<net::RedirectInfo>(), std::string(), original_url,
          original_method, GetCanLoadLocalResources(),
          frame_entry.page_state().ToEncodedData(), GetUniqueID(),
          subframe_unique_names, intended_as_new_entry, pending_offset_to_send,
          current_offset_to_send, current_length_to_send, false,
          IsViewSourceMode(), should_clear_history_list(),
          blink::mojom::NavigationTiming::New(),
          blink::mojom::WasActivatedOption::kUnknown,
          base::UnguessableToken::Create(),
          std::vector<blink::mojom::PrefetchedSignedExchangeInfoPtr>(),
#if BUILDFLAG(IS_ANDROID)
          std::string(),
#endif
          false /* is_browser_initiated */, false /*has_ua_visual_transition*/,
          ukm::kInvalidSourceId /* document_ukm_source_id */, frame_policy,
          std::vector<std::string>() /* force_enabled_origin_trials */,
          false /* origin_agent_cluster */,
          true /* origin_agent_cluster_left_as_default */,
          std::vector<
              network::mojom::WebClientHintsType>() /* enabled_client_hints */,
          false /* is_cross_site_cross_browsing_context_group */,
          false /* should_have_sticky_user_activation */,
          nullptr /* old_page_info */, -1 /* http_response_code */,
          blink::mojom::NavigationApiHistoryEntryArrays::New(),
          std::vector<GURL>() /* early_hints_preloaded_resources */,
          // This timestamp will be populated when the commit IPC is sent.
          base::TimeTicks() /* commit_sent */, std::string() /* srcdoc_value */,
          false /* should_load_data_url */, ancestor_or_self_has_cspee,
          std::string() /* reduced_accept_language */,
          /*navigation_delivery_type=*/
          network::mojom::NavigationDeliveryType::kDefault,
          /*view_transition_state=*/std::nullopt,
          soft_navigation_heuristics_task_id,
          /*modified_runtime_features=*/
          base::flat_map<::blink::mojom::RuntimeFeature, bool>(),
          /*fenced_frame_properties=*/std::nullopt,
          /*not_restored_reasons=*/nullptr,
          /*load_with_storage_access=*/
          net::StorageAccessApiStatus::kNone,
          /*browsing_context_group_info=*/std::nullopt,
          /*lcpp_hint=*/nullptr, blink::CreateDefaultRendererContentSettings(),
          /*cookie_deprecation_label=*/std::nullopt,
          /*visited_link_salt=*/std::nullopt,
          /*local_surface_id=*/std::nullopt);
#if BUILDFLAG(IS_ANDROID)
  // `data_url_as_string` is saved in NavigationEntry but should only be used by
  // main frames, because loadData* navigations can only happen on the main
  // frame.
  bool is_for_main_frame = (root_node()->frame_entry == &frame_entry);
  scoped_refptr<const base::RefCountedString> string = GetDataURLAsString();
  if (is_for_main_frame &&
      NavigationControllerImpl::ValidateDataURLAsString(string)) {
    commit_params->data_url_as_string = string->as_string();
  }
#endif

  commit_params->navigation_timing->system_entropy_at_navigation_start =
      system_entropy_at_navigation_start;

  return commit_params;
}

void NavigationEntryImpl::ResetForCommit(FrameNavigationEntry* frame_entry) {
  // Any state that only matters when a navigation entry is pending should be
  // cleared here.
  // TODO(creis): This state should be moved to NavigationRequest.
  SetPostData(nullptr);
  set_is_renderer_initiated(false);

  set_should_clear_history_list(false);
  set_frame_tree_node_id(FrameTreeNodeId());
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
    UpdatePolicy update_policy,
    int64_t item_sequence_number,
    int64_t document_sequence_number,
    const std::string& navigation_api_key,
    SiteInstanceImpl* site_instance,
    scoped_refptr<SiteInstanceImpl> source_site_instance,
    const GURL& url,
    const std::optional<url::Origin>& origin,
    const Referrer& referrer,
    const std::optional<url::Origin>& initiator_origin,
    const std::optional<GURL>& initiator_base_url,
    const std::vector<GURL>& redirect_chain,
    const blink::PageState& page_state,
    const std::string& method,
    int64_t post_id,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
    std::unique_ptr<PolicyContainerPolicies> policy_container_policies) {
  bool protect_url_in_navigation_api =
      policy_container_policies &&
      NavigationControllerImpl::ShouldProtectUrlInNavigationApi(
          policy_container_policies->referrer_policy);
  // If this is called for the main frame, the FrameNavigationEntry is
  // guaranteed to exist, so just update it directly and return.
  if (frame_tree_node->IsMainFrame()) {
    // If the document of the FrameNavigationEntry is changing, we must clear
    // any child FrameNavigationEntries.
    if (root_node()->frame_entry->document_sequence_number() !=
        document_sequence_number) {
      root_node()->children.clear();
      if (!url.is_empty()) {
        // A cross-document navigation committed in the main frame, so the
        // NavigationEntry loses its "initial NavigationEntry" status. Note that
        // the initial entry creation path also goes through this function, but
        // we know not to remove the status in that case because it uses the
        // empty URL.
        initial_navigation_entry_state_ =
            InitialNavigationEntryState::kNonInitial;
      }
    }

    root_node()->frame_entry->UpdateEntry(
        frame_tree_node->unique_name(), item_sequence_number,
        document_sequence_number, navigation_api_key, site_instance,
        std::move(source_site_instance), url, origin, referrer,
        initiator_origin, initiator_base_url, redirect_chain, page_state,
        method, post_id, std::move(blob_url_loader_factory),
        std::move(policy_container_policies), protect_url_in_navigation_api);
    return;
  }

  // We should already have a TreeNode for the parent node by the time this node
  // commits.  Find it first.
  NavigationEntryImpl::TreeNode* parent_node =
      GetTreeNode(FrameTreeNode::From(frame_tree_node->parent()));
  if (!parent_node) {
    // The renderer should not send a commit for a subframe before its parent.
    // TODO(creis): Kill the renderer if we get here.
    return;
  }

  // Now check whether we have a TreeNode for the node itself.
  const std::string& unique_name = frame_tree_node->unique_name();
  for (const auto& child : parent_node->children) {
    if (child->frame_entry->frame_unique_name() == unique_name) {
      if (update_policy == UpdatePolicy::kReplace) {
        RemoveEntryForFrame(frame_tree_node, false);
        break;
      }
      // If the document of the FrameNavigationEntry is changing, we must clear
      // any child FrameNavigationEntries.
      if (child->frame_entry->document_sequence_number() !=
          document_sequence_number)
        child->children.clear();

      // Update the existing FrameNavigationEntry (e.g., for replaceState).
      child->frame_entry->UpdateEntry(
          unique_name, item_sequence_number, document_sequence_number,
          navigation_api_key, site_instance, std::move(source_site_instance),
          url, origin, referrer, initiator_origin, initiator_base_url,
          redirect_chain, page_state, method, post_id,
          std::move(blob_url_loader_factory),
          std::move(policy_container_policies), protect_url_in_navigation_api);
      return;
    }
  }

  // No entry exists yet, so create a new one.
  // Unordered list, since we expect to look up entries by frame sequence number
  // or unique name.
  auto frame_entry = base::MakeRefCounted<FrameNavigationEntry>(
      unique_name, item_sequence_number, document_sequence_number,
      navigation_api_key, site_instance, std::move(source_site_instance), url,
      origin, referrer, initiator_origin, initiator_base_url, redirect_chain,
      page_state, method, post_id, std::move(blob_url_loader_factory),
      std::move(policy_container_policies), protect_url_in_navigation_api);
  parent_node->children.push_back(
      std::make_unique<NavigationEntryImpl::TreeNode>(parent_node,
                                                      std::move(frame_entry)));
}

FrameNavigationEntry* NavigationEntryImpl::GetFrameEntry(
    FrameTreeNode* frame_tree_node) const {
  NavigationEntryImpl::TreeNode* tree_node = GetTreeNode(frame_tree_node);
  return tree_node ? tree_node->frame_entry.get() : nullptr;
}

void NavigationEntryImpl::ForEachFrameEntry(
    FrameEntryIterationCallback on_frame_entry) {
  NavigationEntryImpl::TreeNode* node = nullptr;
  base::queue<NavigationEntryImpl::TreeNode*> work_queue;
  work_queue.push(root_node());
  while (!work_queue.empty()) {
    node = work_queue.front();
    work_queue.pop();

    on_frame_entry(node->frame_entry.get());

    // Enqueue any children.
    for (const auto& child : node->children)
      work_queue.push(child.get());
  }
}

base::flat_map<std::string, bool> NavigationEntryImpl::GetSubframeUniqueNames(
    FrameTreeNode* frame_tree_node) const {
  base::flat_map<std::string, bool> names;
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
      blink::ExplodedPageState exploded_page_state;
      if (blink::DecodePageState(
              child->frame_entry->page_state().ToEncodedData(),
              &exploded_page_state)) {
        blink::ExplodedFrameState frame_state = exploded_page_state.top;
        if (UTF16ToUTF8(frame_state.url_string.value_or(std::u16string())) ==
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
    auto* frame_entry = node->frame_entry.get();
    if (frame_entry && frame_entry->committed_origin()) {
      // Normally default-isolated origins are tracked through their presence in
      // session history, which is consulted whenever an origin newly requests
      // isolation. If we remove a frame_entry, its origin won't be available
      // to any future global walk if the same origin later wants to opt-in. So
      // we add it to the non-opt-in list here to be spec compliant (unless it's
      // currently opted-in, in which case this call will do nothing).
      ChildProcessSecurityPolicyImpl::GetInstance()
          ->AddDefaultIsolatedOriginIfNeeded(
              frame_entry->site_instance()->GetIsolationContext(),
              frame_entry->committed_origin().value(),
              true /* global_ walk_or_frame_removal */);
    }
    NavigationEntryImpl::TreeNode* parent_node = node->parent;
    auto it = base::ranges::find(
        parent_node->children, node,
        &std::unique_ptr<NavigationEntryImpl::TreeNode>::get);
    CHECK(it != parent_node->children.end());
    parent_node->children.erase(it);
  }
}

void NavigationEntryImpl::UpdateBackForwardCacheNotRestoredReasons(
    NavigationRequest* navigation_request) {
  DCHECK(BackForwardCacheMetrics::IsCrossDocumentMainFrameHistoryNavigation(
      navigation_request));
  if (!back_forward_cache_metrics()) {
    // Create a metrics object if there is none.
    FrameNavigationEntry* frame_navigation_entry =
        GetFrameEntry(navigation_request->frame_tree_node());
    scoped_refptr<BackForwardCacheMetrics> metrics =
        base::WrapRefCounted(new BackForwardCacheMetrics(
            frame_navigation_entry->document_sequence_number()));
    set_back_forward_cache_metrics(std::move(metrics));
  }
  // Update NotRestoredReasons to include additional reasons only
  // known when we navigate back to the NavigationEntry.
  back_forward_cache_metrics()->UpdateNotRestoredReasonsForNavigation(
      navigation_request);
}

GURL NavigationEntryImpl::GetHistoryURLForDataURL() {
  return GetBaseURLForDataURL().is_empty() ? GURL() : GetVirtualURL();
}

}  // namespace content
