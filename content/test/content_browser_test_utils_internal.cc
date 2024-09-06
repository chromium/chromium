// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/content_browser_test_utils_internal.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/containers/stack.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "content/browser/preloading/prerender/prerender_host_registry.h"
#include "content/browser/renderer_host/delegated_frame_host.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_widget_host_factory.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame_messages.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_javascript_dialog_manager.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "third_party/blink/public/common/frame/frame_visual_properties.h"

#if BUILDFLAG(IS_ANDROID)
#include "cc/slim/layer_tree.h"
#include "content/browser/renderer_host/compositor_impl_android.h"
#include "ui/android/window_android.h"
#include "ui/android/window_android_compositor.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC)
#include "content/browser/renderer_host/browser_compositor_view_mac.h"
#include "content/browser/renderer_host/test_render_widget_host_view_mac_factory.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_IOS)
#include "content/browser/renderer_host/browser_compositor_ios.h"
#include "content/browser/renderer_host/test_render_widget_host_view_ios_factory.h"
#endif  // BUILDFLAG(IS_IOS)

#if defined(USE_AURA)
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#endif  // defined(USE_AURA)

namespace content {

bool NavigateFrameToURL(FrameTreeNode* node, const GURL& url) {
  TestFrameNavigationObserver observer(node);
  NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PAGE_TRANSITION_LINK;
  params.frame_tree_node_id = node->frame_tree_node_id();
  FrameTree& frame_tree = node->frame_tree();

  node->navigator().controller().LoadURLWithParams(params);
  observer.Wait();

  if (!observer.last_navigation_succeeded()) {
    DLOG(WARNING) << "Navigation did not succeed: " << url;
    return false;
  }

  // It's possible for JS handlers triggered during the navigation to remove
  // the node, so retrieve it by ID again to check if that occurred.
  node = frame_tree.FindByID(params.frame_tree_node_id);

  if (node && url != node->current_url()) {
    DLOG(WARNING) << "Expected URL " << url << " but observed "
                  << node->current_url();
    return false;
  }
  return true;
}

void SetShouldProceedOnBeforeUnload(Shell* shell, bool proceed, bool success) {
  ShellJavaScriptDialogManager* manager =
      static_cast<ShellJavaScriptDialogManager*>(
          shell->GetJavaScriptDialogManager(shell->web_contents()));
  manager->set_should_proceed_on_beforeunload(proceed, success);
}

RenderFrameHost* ConvertToRenderFrameHost(FrameTreeNode* frame_tree_node) {
  return frame_tree_node->current_frame_host();
}

bool NavigateToURLInSameBrowsingInstance(Shell* window, const GURL& url) {
  TestNavigationObserver observer(window->web_contents());
  // Using a PAGE_TRANSITION_LINK transition with a browser-initiated
  // navigation forces it to stay in the current BrowsingInstance, as normally
  // that transition is used by renderer-initiated navigations.
  window->LoadURLForFrame(url, std::string(),
                          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK));
  observer.Wait();

  if (!IsLastCommittedEntryOfPageType(window->web_contents(),
                                      PAGE_TYPE_NORMAL)) {
    NavigationEntry* last_entry =
        window->web_contents()->GetController().GetLastCommittedEntry();
    DLOG(WARNING) << "last_entry->GetPageType() = "
                  << (last_entry ? last_entry->GetPageType() : -1);
    return false;
  }

  if (window->web_contents()->GetLastCommittedURL() != url) {
    DLOG(WARNING) << "window->web_contents()->GetLastCommittedURL() = "
                  << window->web_contents()->GetLastCommittedURL()
                  << "; url = " << url;
    return false;
  }

  return true;
}

bool IsExpectedSubframeErrorTransition(SiteInstance* start_site_instance,
                                       SiteInstance* end_site_instance) {
  bool site_instances_are_equal = (start_site_instance == end_site_instance);

  // AgentClusterKey mismatch will trigger a SiteInstance switch.
  if (static_cast<SiteInstanceImpl*>(start_site_instance)
              ->GetSiteInfo()
              .agent_cluster_key() !=
          static_cast<SiteInstanceImpl*>(end_site_instance)
              ->GetSiteInfo()
              .agent_cluster_key() &&
      !site_instances_are_equal) {
    return true;
  }

  bool is_error_page_site_instance =
      (static_cast<SiteInstanceImpl*>(end_site_instance)
           ->GetSiteInfo()
           .is_error_page());

  if (!SiteIsolationPolicy::IsErrorPageIsolationEnabled(
          /*in_main_frame=*/false)) {
    return site_instances_are_equal && !is_error_page_site_instance;
  } else {
    return !site_instances_are_equal && is_error_page_site_instance;
  }
}

RenderFrameHost* CreateSubframe(WebContentsImpl* web_contents,
                                std::string frame_id,
                                const GURL& url,
                                bool wait_for_navigation) {
  return CreateSubframe(
      web_contents->GetPrimaryFrameTree().root()->current_frame_host(),
      frame_id, url, wait_for_navigation, {});
}

RenderFrameHost* CreateSubframe(RenderFrameHost* parent,
                                std::string frame_id,
                                const GURL& url,
                                bool wait_for_navigation) {
  return CreateSubframe(parent, frame_id, url, wait_for_navigation, {});
}

RenderFrameHost* CreateSubframe(RenderFrameHost* parent,
                                std::string frame_id,
                                const GURL& url,
                                bool wait_for_navigation,
                                ExtraParams extra_params) {
  WebContents* web_contents = WebContents::FromRenderFrameHost(parent);
  RenderFrameHostCreatedObserver subframe_created_observer(web_contents);
  TestNavigationObserver subframe_nav_observer(web_contents);

  EXPECT_TRUE(
      ExecJs(parent, JsReplace(R"(
    var iframe = document.createElement('iframe');
    iframe.id = $1; //frame_id
    if ($2) {
      iframe.src = $2; // url
    }
    if ($3) {
      iframe.sandbox = $3; // extra_params.sandbox_flags
    }
    document.body.appendChild(iframe);
  )",
                               frame_id, url, extra_params.sandbox_flags)));

  subframe_created_observer.Wait();
  if (wait_for_navigation)
    subframe_nav_observer.Wait();
  FrameTreeNode* root =
      static_cast<RenderFrameHostImpl*>(parent)->frame_tree_node();
  return root->child_at(root->child_count() - 1)->current_frame_host();
}

std::vector<RenderFrameHostImpl*> CollectAllRenderFrameHosts(
    RenderFrameHostImpl* starting_rfh) {
  std::vector<RenderFrameHostImpl*> visited_frames;
  starting_rfh->ForEachRenderFrameHost(
      [&](RenderFrameHostImpl* rfh) { visited_frames.push_back(rfh); });
  return visited_frames;
}

std::vector<RenderFrameHostImpl*>
CollectAllRenderFrameHostsIncludingSpeculative(
    RenderFrameHostImpl* starting_rfh) {
  std::vector<RenderFrameHostImpl*> visited_frames;
  starting_rfh->ForEachRenderFrameHostIncludingSpeculative(
      [&](RenderFrameHostImpl* rfh) { visited_frames.push_back(rfh); });
  return visited_frames;
}

std::vector<RenderFrameHostImpl*> CollectAllRenderFrameHosts(
    WebContentsImpl* web_contents) {
  std::vector<RenderFrameHostImpl*> visited_frames;
  web_contents->ForEachRenderFrameHost(
      [&](RenderFrameHostImpl* rfh) { visited_frames.push_back(rfh); });
  return visited_frames;
}

std::vector<RenderFrameHostImpl*>
CollectAllRenderFrameHostsIncludingSpeculative(WebContentsImpl* web_contents) {
  std::vector<RenderFrameHostImpl*> visited_frames;
  web_contents->ForEachRenderFrameHostIncludingSpeculative(
      [&](RenderFrameHostImpl* rfh) { visited_frames.push_back(rfh); });
  return visited_frames;
}

Shell* OpenBlankWindow(WebContentsImpl* web_contents) {
  FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecJs(root, "last_opened_window = window.open()"));
  Shell* new_shell = new_shell_observer.GetShell();
  EXPECT_NE(new_shell->web_contents(), web_contents);
  EXPECT_TRUE(new_shell->web_contents()
                  ->GetController()
                  .GetLastCommittedEntry()
                  ->IsInitialEntry());
  EXPECT_EQ(1, new_shell->web_contents()->GetController().GetEntryCount());
  return new_shell;
}

Shell* OpenWindow(WebContentsImpl* web_contents, const GURL& url) {
  FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(
      ExecJs(root, JsReplace("last_opened_window = window.open($1)", url)));
  Shell* new_shell = new_shell_observer.GetShell();
  EXPECT_NE(new_shell->web_contents(), web_contents);
  return new_shell;
}

FrameTreeVisualizer::FrameTreeVisualizer() = default;

FrameTreeVisualizer::~FrameTreeVisualizer() = default;

std::string FrameTreeVisualizer::DepictFrameTree(FrameTreeNode* root) {
  // Tracks the sites actually used in this depiction.
  std::map<std::string, SiteInstance*> legend;

  // Traversal 1: Assign names to current frames. This ensures that the first
  // call to the pretty-printer will result in a naming of the site instances
  // that feels natural and stable.
  base::stack<FrameTreeNode*> to_explore;
  for (to_explore.push(root); !to_explore.empty();) {
    FrameTreeNode* node = to_explore.top();
    to_explore.pop();
    for (size_t i = node->child_count(); i-- != 0;) {
      to_explore.push(node->child_at(i));
    }

    RenderFrameHost* current = node->render_manager()->current_frame_host();
    legend[GetName(current->GetSiteInstance())] = current->GetSiteInstance();
  }

  // Traversal 2: Assign names to the pending/speculative frames. For stability
  // of assigned names it's important to do this before trying to name the
  // proxies, which have a less well defined order.
  for (to_explore.push(root); !to_explore.empty();) {
    FrameTreeNode* node = to_explore.top();
    to_explore.pop();
    for (size_t i = node->child_count(); i-- != 0;) {
      to_explore.push(node->child_at(i));
    }

    RenderFrameHost* spec = node->render_manager()->speculative_frame_host();
    if (spec)
      legend[GetName(spec->GetSiteInstance())] = spec->GetSiteInstance();
  }

  // Traversal 3: Assign names to the SiteInstances within each group's proxies
  // (which are associated with SiteInstanceGroups instead of SiteInstances) and
  // add them to |legend| too. Typically, only openers should have their names
  // assigned this way.
  for (to_explore.push(root); !to_explore.empty();) {
    FrameTreeNode* node = to_explore.top();
    to_explore.pop();
    for (size_t i = node->child_count(); i-- != 0;) {
      to_explore.push(node->child_at(i));
    }

    // Sort the proxies by SiteInstanceGroup ID to avoid unordered_map ordering.
    std::vector<SiteInstance*> site_instances;
    for (const auto& proxy_pair :
         node->render_manager()->GetAllProxyHostsForTesting()) {
      SiteInstanceGroup* group = proxy_pair.second->site_instance_group();
      for (raw_ptr<SiteInstanceImpl> instance :
           group->site_instances_for_testing()) {
        site_instances.push_back(instance);
      }
    }
    std::sort(site_instances.begin(), site_instances.end(),
              [](SiteInstance* lhs, SiteInstance* rhs) {
                return lhs->GetId() < rhs->GetId();
              });

    for (SiteInstance* site_instance : site_instances)
      legend[GetName(site_instance)] = site_instance;
  }

  // Traversal 4: Now that all names are assigned, make a big loop to pretty-
  // print the tree. Each iteration produces exactly one line of format.
  std::string result;
  for (to_explore.push(root); !to_explore.empty();) {
    FrameTreeNode* node = to_explore.top();
    to_explore.pop();
    for (size_t i = node->child_count(); i-- != 0;) {
      to_explore.push(node->child_at(i));
    }

    // Draw the feeler line tree graphics by walking up to the root. A feeler
    // line is needed for each ancestor that is the last child of its parent.
    // This creates the ASCII art that looks like:
    //    Foo
    //      |--Foo
    //      |--Foo
    //      |    |--Foo
    //      |    +--Foo
    //      |         +--Foo
    //      +--Foo
    //           +--Foo
    //
    // TODO(nick): Make this more elegant.
    std::string line;
    if (node != root) {
      if (node->parent()->child_at(node->parent()->child_count() - 1) != node)
        line = "  |--";
      else
        line = "  +--";
      for (RenderFrameHostImpl* up = node->parent();
           up != root->current_frame_host(); up = up->GetParent()) {
        if (up->GetParent()
                ->child_at(up->GetParent()->child_count() - 1)
                ->current_frame_host() != up)
          line = "  |  " + line;
        else
          line = "     " + line;
      }
    }

    // Prefix one extra space of padding for two reasons. First, this helps the
    // diagram aligns nicely with the legend. Second, this makes it easier to
    // read the diffs that gtest spits out on EXPECT_EQ failure.
    line = " " + line;

    // Summarize the FrameTreeNode's state. Always show the site of the current
    // RenderFrameHost, and show any exceptional state of the node, like a
    // pending or speculative RenderFrameHost.
    RenderFrameHost* current = node->render_manager()->current_frame_host();
    RenderFrameHost* spec = node->render_manager()->speculative_frame_host();
    base::StringAppendF(&line, "Site %s",
                        GetName(current->GetSiteInstance()).c_str());
    if (spec) {
      base::StringAppendF(&line, " (%s speculative)",
                          GetName(spec->GetSiteInstance()).c_str());
    }

    // Show the SiteInstances of the RenderFrameProxyHosts of this node.
    const auto& proxy_host_map =
        node->render_manager()->GetAllProxyHostsForTesting();
    if (!proxy_host_map.empty()) {
      // Show a dashed line of variable length before the proxy list. Always at
      // least two dashes.
      line.append(" --");

      // To make proxy lists align vertically for the first three tree levels,
      // pad with dashes up to a first tab stop at column 19 (which works out to
      // text editor column 28 in the typical diagram fed to EXPECT_EQ as a
      // string literal). Lining the lists up vertically makes differences in
      // the proxy sets easier to spot visually. We choose not to use the
      // *actual* tree height here, because that would make the diagram's
      // appearance less stable as the tree's shape evolves.
      while (line.length() < 20) {
        line.append("-");
      }
      line.append(" proxies for");

      // Sort these alphabetically, to avoid hash_map ordering dependency.
      std::vector<std::string> sorted_proxy_hosts;
      for (const auto& proxy_pair : proxy_host_map) {
        sorted_proxy_hosts.push_back(
            GetGroupName(proxy_pair.second->site_instance_group()));
      }
      std::sort(sorted_proxy_hosts.begin(), sorted_proxy_hosts.end());
      for (std::string& proxy_name : sorted_proxy_hosts) {
        base::StringAppendF(&line, " %s", proxy_name.c_str());
      }
    }
    if (node != root)
      result.append("\n");
    result.append(line);
  }

  // Finally, show a legend with details of the site instances.
  const char* prefix = "Where ";
  for (auto& legend_entry : legend) {
    SiteInstanceImpl* site_instance =
        static_cast<SiteInstanceImpl*>(legend_entry.second);
    std::string description =
        GetUrlWithoutPort(site_instance->GetSiteURL()).spec();

    // data: URLs have site URLs of the form data:nonce, where the nonce is an
    // UnguessableToken. Make these deterministic for testing by using the
    // abbreviated letter for the site in the nonce. For example,
    // "data:nonce_A".
    if (site_instance->GetSiteURL().SchemeIs(url::kDataScheme)) {
      description =
          base::StringPrintf("data:nonce_%s", legend_entry.first.c_str());
    }

    base::StringAppendF(&result, "\n%s%s = %s", prefix,
                        legend_entry.first.c_str(), description.c_str());
    // Highlight some exceptionable conditions.
    if (site_instance->GetSiteInfo().is_sandboxed()) {
      result.append(" (sandboxed)");
    }
    if (site_instance->group()->active_frame_count() == 0)
      result.append(" (active_frame_count == 0)");
    if (!site_instance->GetProcess()->IsInitializedAndNotDead())
      result.append(" (no process)");
    prefix = "      ";
  }
  return result;
}

std::string FrameTreeVisualizer::GetName(SiteInstance* site_instance) {
  // Indices into the vector correspond to letters of the alphabet.
  size_t index =
      base::ranges::find(seen_site_instance_ids_, site_instance->GetId()) -
      seen_site_instance_ids_.begin();
  if (index == seen_site_instance_ids_.size())
    seen_site_instance_ids_.push_back(site_instance->GetId());

  // Whosoever writes a test using >=26 site instances shall be a lucky ducky.
  if (index < 25)
    return base::StringPrintf("%c", 'A' + static_cast<char>(index));
  else
    return base::StringPrintf("Z%d", static_cast<int>(index - 25));
}

std::string FrameTreeVisualizer::GetGroupName(SiteInstanceGroup* group) {
  // If there's only one SiteInstance in `group`, get the name of the
  // SiteInstance directly. This preserves test expectations for DepictFrameTree
  // uses that predate SiteInstanceGroup.
  if (group->site_instances_for_testing().size() == 1) {
    return GetName(*group->site_instances_for_testing().begin());
  }

  // Alphabetically sort the SiteInstances within the group.
  std::vector<std::string> sorted_instance_names;
  for (auto& site_instance : group->site_instances_for_testing()) {
    sorted_instance_names.push_back(GetName(site_instance));
  }
  std::sort(sorted_instance_names.begin(), sorted_instance_names.end());

  // Name the group using set notation.
  CHECK(sorted_instance_names.size() >= 1u);
  std::string result = "{";
  for (auto& site_instance_name : sorted_instance_names) {
    base::StringAppendF(&result, "%s,", site_instance_name.c_str());
  }
  result.resize(result.length() - 1);
  result.append("}");

  return result;
}

GURL FrameTreeVisualizer::GetUrlWithoutPort(const GURL& url) {
  GURL::Replacements replacements;
  replacements.ClearPort();
  return url.ReplaceComponents(replacements);
}

std::string DepictFrameTree(FrameTreeNode& root) {
  return FrameTreeVisualizer().DepictFrameTree(&root);
}

Shell* OpenPopup(const ToRenderFrameHost& opener,
                 const GURL& url,
                 const std::string& name) {
  return OpenPopup(opener, url, name, "", true);
}

Shell* OpenPopup(const ToRenderFrameHost& opener,
                 const GURL& url,
                 const std::string& name,
                 const std::string& features,
                 bool expect_return_from_window_open) {
  TestNavigationObserver observer(url);
  observer.StartWatchingNewWebContents();

  ShellAddedObserver new_shell_observer;
  std::string popup_script = "!!window.open('" + url.spec() + "', '" + name +
                             "', '" + features + "');";
  bool did_create_popup = EvalJs(opener, popup_script).ExtractBool();

  if (!(did_create_popup || !expect_return_from_window_open)) {
    return nullptr;
  }

  observer.Wait();

  Shell* new_shell = new_shell_observer.GetShell();
  EXPECT_EQ(
      url,
      new_shell->web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL());
  return new_shell_observer.GetShell();
}

FileChooserDelegate::FileChooserDelegate(std::vector<base::FilePath> files,
                                         const base::FilePath& base_dir,
                                         base::OnceClosure callback)
    : files_(std::move(files)),
      base_dir_(base_dir),
      callback_(std::move(callback)) {}

FileChooserDelegate::FileChooserDelegate(const base::FilePath& file,
                                         base::OnceClosure callback)
    : FileChooserDelegate(std::vector<base::FilePath>(1, file),
                          base::FilePath(),
                          std::move(callback)) {}

FileChooserDelegate::~FileChooserDelegate() = default;

void FileChooserDelegate::RunFileChooser(
    RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  // |base_dir_| should be set for and only for |kUploadFolder| mode.
  DCHECK(base_dir_.empty() ==
         (params.mode != blink::mojom::FileChooserParams::Mode::kUploadFolder));
  // Send the selected files to the renderer process.
  std::vector<blink::mojom::FileChooserFileInfoPtr> files;
  for (const auto& file : files_) {
    auto file_info = blink::mojom::FileChooserFileInfo::NewNativeFile(
        blink::mojom::NativeFileInfo::New(file, std::u16string()));
    files.push_back(std::move(file_info));
  }
  listener->FileSelected(std::move(files), base_dir_, params.mode);

  params_ = params.Clone();
  if (callback_)
    std::move(callback_).Run();
}

FrameTestNavigationManager::FrameTestNavigationManager(
    FrameTreeNodeId filtering_frame_tree_node_id,
    WebContents* web_contents,
    const GURL& url)
    : TestNavigationManager(web_contents, url),
      filtering_frame_tree_node_id_(filtering_frame_tree_node_id) {}

bool FrameTestNavigationManager::ShouldMonitorNavigation(
    NavigationHandle* handle) {
  return TestNavigationManager::ShouldMonitorNavigation(handle) &&
         handle->GetFrameTreeNodeId() == filtering_frame_tree_node_id_;
}

UrlCommitObserver::UrlCommitObserver(FrameTreeNode* frame_tree_node,
                                     const GURL& url)
    : content::WebContentsObserver(WebContents::FromRenderFrameHost(
          frame_tree_node->current_frame_host())),
      frame_tree_node_id_(frame_tree_node->frame_tree_node_id()),
      url_(url) {}

UrlCommitObserver::~UrlCommitObserver() {}

void UrlCommitObserver::Wait() {
  run_loop_.Run();
}

void UrlCommitObserver::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (navigation_handle->HasCommitted() &&
      !navigation_handle->IsErrorPage() &&
      navigation_handle->GetURL() == url_ &&
      navigation_handle->GetFrameTreeNodeId() == frame_tree_node_id_) {
    run_loop_.Quit();
  }
}

RenderProcessHostBadIpcMessageWaiter::RenderProcessHostBadIpcMessageWaiter(
    RenderProcessHost* render_process_host)
    : internal_waiter_(render_process_host,
                       "Stability.BadMessageTerminated.Content") {}

std::optional<bad_message::BadMessageReason>
RenderProcessHostBadIpcMessageWaiter::Wait() {
  std::optional<int> internal_result = internal_waiter_.Wait();
  if (!internal_result.has_value())
    return std::nullopt;
  return static_cast<bad_message::BadMessageReason>(internal_result.value());
}

CreateNewPopupWidgetInterceptor::CreateNewPopupWidgetInterceptor(
    RenderFrameHostImpl* rfh,
    base::OnceCallback<void(RenderWidgetHostImpl*)> did_create_callback)
    : swapped_impl_(rfh->local_frame_host_receiver_for_testing(), this),
      did_create_callback_(std::move(did_create_callback)) {}

CreateNewPopupWidgetInterceptor::~CreateNewPopupWidgetInterceptor() = default;

void CreateNewPopupWidgetInterceptor::CreateNewPopupWidget(
    mojo::PendingAssociatedReceiver<blink::mojom::PopupWidgetHost>
        blink_popup_widget_host,
    mojo::PendingAssociatedReceiver<blink::mojom::WidgetHost> blink_widget_host,
    mojo::PendingAssociatedRemote<blink::mojom::Widget> blink_widget) {
  class PopupWidgetCreationObserver : public RenderWidgetHostFactory {
   public:
    PopupWidgetCreationObserver() { RegisterFactory(this); }

    ~PopupWidgetCreationObserver() override { UnregisterFactory(); }

    // RenderWidgetHostFactory overrides:
    RenderWidgetHostImpl* CreateSelfOwnedRenderWidgetHost(
        FrameTree* frame_tree,
        RenderWidgetHostDelegate* delegate,
        base::SafeRef<SiteInstanceGroup> site_instance_group,
        int32_t routing_id,
        bool hidden) override {
      CHECK(!last_created_widget_);
      last_created_widget_ =
          RenderWidgetHostFactory::CreateSelfOwnedRenderWidgetHost(
              frame_tree, delegate, std::move(site_instance_group), routing_id,
              hidden);
      return last_created_widget_;
    }

    RenderWidgetHostImpl* TakeLastCreatedWidget() {
      return std::exchange(last_created_widget_, nullptr);
    }

   private:
    raw_ptr<RenderWidgetHostImpl> last_created_widget_;
  };

  PopupWidgetCreationObserver creation_observer;

  GetForwardingInterface()->CreateNewPopupWidget(
      std::move(blink_popup_widget_host), std::move(blink_widget_host),
      std::move(blink_widget));

  if (!did_create_callback_) {
    return;
  }

  if (auto* widget = creation_observer.TakeLastCreatedWidget(); widget) {
    std::move(did_create_callback_).Run(widget);
  }
}

blink::mojom::LocalFrameHost*
CreateNewPopupWidgetInterceptor::GetForwardingInterface() {
  return swapped_impl_.old_impl();
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
ShowPopupWidgetWaiter::ShowPopupMenuInterceptor::ShowPopupMenuInterceptor(
    RenderFrameHostImpl* rfh,
    base::OnceCallback<void(const gfx::Rect&)> did_show_popup_menu_callback)
    : swapped_impl_(rfh->local_frame_host_receiver_for_testing(), this),
      did_show_popup_menu_callback_(std::move(did_show_popup_menu_callback)) {}

ShowPopupWidgetWaiter::ShowPopupMenuInterceptor::~ShowPopupMenuInterceptor() =
    default;

void ShowPopupWidgetWaiter::ShowPopupMenuInterceptor::ShowPopupMenu(
    mojo::PendingRemote<blink::mojom::PopupMenuClient> popup_client,
    const gfx::Rect& bounds,
    int32_t item_height,
    double font_size,
    int32_t selected_item,
    std::vector<blink::mojom::MenuItemPtr> menu_items,
    bool right_aligned,
    bool allow_multiple_selection) {
  if (did_show_popup_menu_callback_) {
    std::move(did_show_popup_menu_callback_).Run(bounds);
    mojo::Remote<blink::mojom::PopupMenuClient>(std::move(popup_client))
        ->DidCancel();
    return;
  }

  GetForwardingInterface()->ShowPopupMenu(
      std::move(popup_client), bounds, item_height, font_size, selected_item,
      std::move(menu_items), right_aligned, allow_multiple_selection);
}

blink::mojom::LocalFrameHost*
ShowPopupWidgetWaiter::ShowPopupMenuInterceptor::GetForwardingInterface() {
  return swapped_impl_.old_impl();
}
#endif

ShowPopupWidgetWaiter::ShowPopupWidgetWaiter(WebContentsImpl* web_contents,
                                             RenderFrameHostImpl* frame_host)
    : create_new_popup_widget_interceptor_(
          frame_host,
          base::BindOnce(&ShowPopupWidgetWaiter::DidCreatePopupWidget,
                         base::Unretained(this))),
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
      show_popup_menu_interceptor_(
          frame_host,
          base::BindOnce(&ShowPopupWidgetWaiter::DidShowPopupMenu,
                         base::Unretained(this))),
#endif

      frame_host_(frame_host) {
}

ShowPopupWidgetWaiter::~ShowPopupWidgetWaiter() {
  if (auto* rwhi = RenderWidgetHostImpl::FromID(process_id_, routing_id_)) {
    std::ignore =
        rwhi->popup_widget_host_receiver_for_testing().SwapImplForTesting(rwhi);
  }
}

void ShowPopupWidgetWaiter::Wait() {
  run_loop_.Run();
}

blink::mojom::PopupWidgetHost* ShowPopupWidgetWaiter::GetForwardingInterface() {
  DCHECK_NE(MSG_ROUTING_NONE, routing_id_);
  return RenderWidgetHostImpl::FromID(process_id_, routing_id_);
}

void ShowPopupWidgetWaiter::ShowPopup(const gfx::Rect& initial_rect,
                                      const gfx::Rect& initial_anchor_rect,
                                      ShowPopupCallback callback) {
  GetForwardingInterface()->ShowPopup(initial_rect, initial_anchor_rect,
                                      std::move(callback));
  initial_rect_ = initial_rect;
  run_loop_.Quit();
}

void ShowPopupWidgetWaiter::DidCreatePopupWidget(
    RenderWidgetHostImpl* render_widget_host) {
  process_id_ = render_widget_host->GetProcess()->GetID();
  routing_id_ = render_widget_host->GetRoutingID();
  // Swapped back in destructor from process_id_ and routing_id_ lookup.
  std::ignore = render_widget_host->popup_widget_host_receiver_for_testing()
                    .SwapImplForTesting(this);
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
void ShowPopupWidgetWaiter::DidShowPopupMenu(const gfx::Rect& bounds) {
  initial_rect_ = bounds;
  run_loop_.Quit();
}
#endif

UnresponsiveRendererObserver::UnresponsiveRendererObserver(
    WebContents* web_contents)
    : WebContentsObserver(web_contents) {}

UnresponsiveRendererObserver::~UnresponsiveRendererObserver() = default;

RenderProcessHost* UnresponsiveRendererObserver::Wait(base::TimeDelta timeout) {
  if (!captured_render_process_host_) {
    base::OneShotTimer timer;
    timer.Start(FROM_HERE, timeout, run_loop_.QuitClosure());
    run_loop_.Run();
    timer.Stop();
  }
  return captured_render_process_host_;
}

void UnresponsiveRendererObserver::OnRendererUnresponsive(
    RenderProcessHost* render_process_host) {
  captured_render_process_host_ = render_process_host;
  run_loop_.Quit();
}

BeforeUnloadBlockingDelegate::BeforeUnloadBlockingDelegate(
    WebContentsImpl* web_contents)
    : web_contents_(web_contents) {
  web_contents_->SetDelegate(this);
}

BeforeUnloadBlockingDelegate::~BeforeUnloadBlockingDelegate() {
  if (!callback_.is_null())
    std::move(callback_).Run(true, std::u16string());

  web_contents_->SetDelegate(nullptr);
  web_contents_->SetJavaScriptDialogManagerForTesting(nullptr);
}

void BeforeUnloadBlockingDelegate::Wait() {
  run_loop_->Run();
  run_loop_ = std::make_unique<base::RunLoop>();
}

JavaScriptDialogManager*
BeforeUnloadBlockingDelegate::GetJavaScriptDialogManager(WebContents* source) {
  return this;
}

bool BeforeUnloadBlockingDelegate::IsBackForwardCacheSupported(
    WebContents& web_contents) {
  return true;
}

void BeforeUnloadBlockingDelegate::RunJavaScriptDialog(
    WebContents* web_contents,
    RenderFrameHost* render_frame_host,
    JavaScriptDialogType dialog_type,
    const std::u16string& message_text,
    const std::u16string& default_prompt_text,
    DialogClosedCallback callback,
    bool* did_suppress_message) {
  NOTREACHED_IN_MIGRATION();
}

void BeforeUnloadBlockingDelegate::RunBeforeUnloadDialog(
    WebContents* web_contents,
    RenderFrameHost* render_frame_host,
    bool is_reload,
    DialogClosedCallback callback) {
  callback_ = std::move(callback);
  run_loop_->Quit();
}

bool BeforeUnloadBlockingDelegate::HandleJavaScriptDialog(
    WebContents* web_contents,
    bool accept,
    const std::u16string* prompt_override) {
  NOTREACHED_IN_MIGRATION();
  return true;
}

FrameNavigateParamsCapturer::FrameNavigateParamsCapturer(WebContents* contents)
    : WebContentsObserver(contents) {}

FrameNavigateParamsCapturer::FrameNavigateParamsCapturer(FrameTreeNode* node)
    : WebContentsObserver(
          WebContents::FromRenderFrameHost(node->current_frame_host())),
      frame_tree_node_id_(node->frame_tree_node_id()) {}

FrameNavigateParamsCapturer::~FrameNavigateParamsCapturer() = default;

void FrameNavigateParamsCapturer::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() ||
      (frame_tree_node_id_.has_value() &&
       navigation_handle->GetFrameTreeNodeId() !=
           frame_tree_node_id_.value()) ||
      navigations_remaining_ == 0) {
    return;
  }

  --navigations_remaining_;
  transitions_.push_back(navigation_handle->GetPageTransition());
  urls_.push_back(navigation_handle->GetURL());
  navigation_types_.push_back(
      NavigationRequest::From(navigation_handle)->navigation_type());
  is_same_documents_.push_back(navigation_handle->IsSameDocument());
  did_replace_entries_.push_back(navigation_handle->DidReplaceEntry());
  is_renderer_initiateds_.push_back(navigation_handle->IsRendererInitiated());
  has_user_gestures_.push_back(navigation_handle->HasUserGesture());
  is_overriding_user_agents_.push_back(
      NavigationRequest::From(navigation_handle)->is_overriding_user_agent());
  is_error_pages_.push_back(navigation_handle->IsErrorPage());
  if (!navigations_remaining_ &&
      (!web_contents()->IsLoading() || !wait_for_load_))
    loop_.Quit();
}

void FrameNavigateParamsCapturer::Wait() {
  loop_.Run();
}

void FrameNavigateParamsCapturer::DidStopLoading() {
  if (!navigations_remaining_)
    loop_.Quit();
}

RenderFrameHostCreatedObserver::RenderFrameHostCreatedObserver(
    WebContents* web_contents)
    : WebContentsObserver(web_contents) {}

RenderFrameHostCreatedObserver::RenderFrameHostCreatedObserver(
    WebContents* web_contents,
    int expected_frame_count)
    : WebContentsObserver(web_contents),
      expected_frame_count_(expected_frame_count) {}

RenderFrameHostCreatedObserver::RenderFrameHostCreatedObserver(
    WebContents* web_contents,
    OnRenderFrameHostCreatedCallback on_rfh_created)
    : WebContentsObserver(web_contents),
      on_rfh_created_(std::move(on_rfh_created)) {}

RenderFrameHostCreatedObserver::~RenderFrameHostCreatedObserver() = default;

RenderFrameHost* RenderFrameHostCreatedObserver::Wait() {
  if (frames_created_ < expected_frame_count_)
    run_loop_.Run();

  return last_rfh_;
}

void RenderFrameHostCreatedObserver::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  frames_created_++;
  last_rfh_ = render_frame_host;
  if (on_rfh_created_)
    on_rfh_created_.Run(render_frame_host);
  if (frames_created_ == expected_frame_count_)
    run_loop_.Quit();
}

BackForwardCache::DisabledReason RenderFrameHostDisabledForTestingReason() {
  static const BackForwardCache::DisabledReason reason =
      BackForwardCache::DisabledReason(
          BackForwardCache::DisabledSource::kTesting, 0, "disabled for testing",
          /*context=*/"", "disabled");
  return reason;
}

void DisableBFCacheForRFHForTesting(
    content::RenderFrameHost* render_frame_host) {
  content::BackForwardCache::DisableForRenderFrameHost(
      render_frame_host, RenderFrameHostDisabledForTestingReason());
}

void DisableBFCacheForRFHForTesting(content::GlobalRenderFrameHostId id) {
  content::BackForwardCache::DisableForRenderFrameHost(
      id, RenderFrameHostDisabledForTestingReason());
}

void UserAgentInjector::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  web_contents()->SetUserAgentOverride(user_agent_override_, false);
  navigation_handle->SetIsOverridingUserAgent(is_overriding_user_agent_);
}

RenderFrameHostImplWrapper::RenderFrameHostImplWrapper(RenderFrameHost* rfh)
    : RenderFrameHostWrapper(rfh) {}

RenderFrameHostImpl* RenderFrameHostImplWrapper::get() const {
  return static_cast<RenderFrameHostImpl*>(RenderFrameHostWrapper::get());
}

RenderFrameHostImpl& RenderFrameHostImplWrapper::operator*() const {
  DCHECK(get());
  return *get();
}

RenderFrameHostImpl* RenderFrameHostImplWrapper::operator->() const {
  DCHECK(get());
  return get();
}

InactiveRenderFrameHostDeletionObserver::
    InactiveRenderFrameHostDeletionObserver(WebContents* content)
    : WebContentsObserver(content) {}

InactiveRenderFrameHostDeletionObserver::
    ~InactiveRenderFrameHostDeletionObserver() = default;

void InactiveRenderFrameHostDeletionObserver::Wait() {
  // Some RenderFrameHost may remain in the BackForwardCache and or as
  // prerendered pages. Trigger deletion for them asynchronously.
  static_cast<WebContentsImpl*>(web_contents())
      ->GetController()
      .GetBackForwardCache()
      .Flush();
  static_cast<WebContentsImpl*>(web_contents())
      ->GetPrerenderHostRegistry()
      ->CancelAllHostsForTesting();

  for (RenderFrameHost* rfh : CollectAllRenderFrameHosts(web_contents())) {
    // Keep track of all currently inactive RenderFrameHosts so that we can wait
    // for all of them to be deleted.
    if (!rfh->IsActive() && rfh->IsRenderFrameLive())
      inactive_rfhs_.insert(rfh);
  }
  loop_ = std::make_unique<base::RunLoop>();
  CheckCondition();
  loop_->Run();
}

void InactiveRenderFrameHostDeletionObserver::RenderFrameDeleted(
    RenderFrameHost* rfh) {
  if (inactive_rfhs_.count(rfh) == 0)
    return;
  inactive_rfhs_.erase(rfh);
  CheckCondition();
}

void InactiveRenderFrameHostDeletionObserver::CheckCondition() {
  if (loop_ && inactive_rfhs_.empty())
    loop_->Quit();
}

void TestNavigationObserverInternal::OnDidFinishNavigation(
    NavigationHandle* navigation_handle) {
  last_navigation_type_ =
      navigation_handle->HasCommitted()
          ? static_cast<NavigationRequest*>(navigation_handle)
                ->navigation_type()
          : NAVIGATION_TYPE_UNKNOWN;
  TestNavigationObserver::OnDidFinishNavigation(navigation_handle);
}

RenderFrameHostImpl* DescendantRenderFrameHostAtInternal(
    RenderFrameHostImpl* rfh,
    std::string path,
    std::vector<size_t>& descendant_indices) {
  if (descendant_indices.size() == 0)
    return rfh;
  size_t index = descendant_indices[0];
  descendant_indices.erase(descendant_indices.begin());
  CHECK_LT(index, rfh->child_count()) << path;
  FrameTreeNode* node = rfh->child_at(index);
  path = base::StringPrintf("%s[%zu]", path.c_str(), index);
  return DescendantRenderFrameHostAtInternal(node->current_frame_host(), path,
                                             descendant_indices);
}

RenderFrameHostImpl* DescendantRenderFrameHostImplAt(
    const ToRenderFrameHost& adapter,
    std::vector<size_t> descendant_indices) {
  return DescendantRenderFrameHostAtInternal(
      static_cast<RenderFrameHostImpl*>(adapter.render_frame_host()), "rfh",
      descendant_indices);
}

EffectiveURLContentBrowserTestContentBrowserClient::
    EffectiveURLContentBrowserTestContentBrowserClient(
        bool requires_dedicated_process)
    : helper_(requires_dedicated_process) {}

EffectiveURLContentBrowserTestContentBrowserClient::
    EffectiveURLContentBrowserTestContentBrowserClient(
        const GURL& url_to_modify,
        const GURL& url_to_return,
        bool requires_dedicated_process)
    : helper_(requires_dedicated_process) {
  AddTranslation(url_to_modify, url_to_return);
}

EffectiveURLContentBrowserTestContentBrowserClient::
    ~EffectiveURLContentBrowserTestContentBrowserClient() = default;

void EffectiveURLContentBrowserTestContentBrowserClient::AddTranslation(
    const GURL& url_to_modify,
    const GURL& url_to_return) {
  helper_.AddTranslation(url_to_modify, url_to_return);
}

GURL EffectiveURLContentBrowserTestContentBrowserClient::GetEffectiveURL(
    BrowserContext* browser_context,
    const GURL& url) {
  return helper_.GetEffectiveURL(url);
}

bool EffectiveURLContentBrowserTestContentBrowserClient::
    DoesSiteRequireDedicatedProcess(BrowserContext* browser_context,
                                    const GURL& effective_site_url) {
  return helper_.DoesSiteRequireDedicatedProcess(browser_context,
                                                 effective_site_url);
}

CustomStoragePartitionBrowserClient::CustomStoragePartitionBrowserClient(
    const GURL& site_to_isolate)
    : site_to_isolate_(site_to_isolate) {}

StoragePartitionConfig
CustomStoragePartitionBrowserClient::GetStoragePartitionConfigForSite(
    BrowserContext* browser_context,
    const GURL& site) {
  // Override for |site_to_isolate_|.
  if (site == site_to_isolate_) {
    return StoragePartitionConfig::Create(
        browser_context, "blah_isolated_storage", "blah_isolated_storage",
        false /* in_memory */);
  }

  return StoragePartitionConfig::CreateDefault(browser_context);
}

CommitNavigationPauser::CommitNavigationPauser(RenderFrameHostImpl* rfh) {
  rfh->SetCommitCallbackInterceptorForTesting(this);
}

CommitNavigationPauser::~CommitNavigationPauser() = default;

void CommitNavigationPauser::WaitForCommitAndPause() {
  loop_.Run();
}

void CommitNavigationPauser::ResumePausedCommit() {
  // The caller is responsible for ensuring the paused request is still alive
  // and not discarded.
  DCHECK(paused_request_);
  paused_request_->GetRenderFrameHost()->DidCommitNavigation(
      paused_request_.get(), std::move(paused_params_),
      std::move(paused_interface_params_));
}

bool CommitNavigationPauser::WillProcessDidCommitNavigation(
    NavigationRequest* request,
    mojom::DidCommitProvisionalLoadParamsPtr* params,
    mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params) {
  request->GetRenderFrameHost()->SetCommitCallbackInterceptorForTesting(
      nullptr);

  paused_request_ = request->GetWeakPtr();
  paused_params_ = std::move(*params);
  paused_interface_params_ = std::move(*interface_params);

  loop_.Quit();

  // Ignore the commit message.
  return false;
}

// TODO(crbug.com/40278950): Use
// `WebFrameWidgetImpl::NotifySwapAndPresentationTime` instead.
void WaitForCopyableViewInWebContents(WebContents* web_contents) {
  WaitForCopyableViewInFrame(web_contents->GetPrimaryMainFrame());
}

void WaitForCopyableViewInFrame(RenderFrameHost* render_frame_host) {
  base::test::TestFuture<void> future;
  NotifyCopyableViewInFrame(render_frame_host, future.GetCallback());
  CHECK(future.Wait());
}

void WaitForBrowserCompositorFramePresented(WebContents* web_contents) {
  base::RunLoop run_loop;
  auto callback = base::BindOnce(
      [](base::RepeatingClosure cb,
         const viz::FrameTimingDetails& frame_timing_details) {
        std::move(cb).Run();
      },
      run_loop.QuitClosure());
#if BUILDFLAG(IS_ANDROID)
  ui::WindowAndroidCompositor* compositor =
      web_contents->GetNativeView()->GetWindowAndroid()->GetCompositor();
  compositor->PostRequestSuccessfulPresentationTimeForNextFrame(
      std::move(callback));
#elif BUILDFLAG(IS_MAC)
  auto* browser_compositor = GetBrowserCompositorMacForTesting(
      web_contents->GetRenderWidgetHostView());
  browser_compositor->GetCompositor()
      ->RequestSuccessfulPresentationTimeForNextFrame(std::move(callback));
#elif BUILDFLAG(IS_IOS)
  auto* browser_compositor = GetBrowserCompositorIOSForTesting(
      web_contents->GetRenderWidgetHostView());
  browser_compositor->GetCompositor()
      ->RequestSuccessfulPresentationTimeForNextFrame(std::move(callback));
#elif defined(USE_AURA)
  auto* compositor = static_cast<RenderWidgetHostViewAura*>(
                         web_contents->GetRenderWidgetHostView())
                         ->GetCompositor();
  compositor->RequestSuccessfulPresentationTimeForNextFrame(
      std::move(callback));
#else
  NOTREACHED_IN_MIGRATION();
#endif
  run_loop.Run();
}

void ForceNewCompositorFrameFromBrowser(WebContents* web_contents) {
#if BUILDFLAG(IS_ANDROID)
  ui::WindowAndroid* window = web_contents->GetTopLevelNativeWindow();
  ui::WindowAndroidCompositor* compositor = window->GetCompositor();
  cc::slim::LayerTree* layer_tree =
      static_cast<CompositorImpl*>(compositor)->GetLayerTreeForTesting();
  layer_tree->SetNeedsRedrawForTesting();
#elif BUILDFLAG(IS_MAC)
  auto* browser_compositor = GetBrowserCompositorMacForTesting(
      web_contents->GetRenderWidgetHostView());
  browser_compositor->GetCompositor()->ScheduleFullRedraw();
#elif BUILDFLAG(IS_IOS)
  auto* browser_compositor = GetBrowserCompositorIOSForTesting(
      web_contents->GetRenderWidgetHostView());
  browser_compositor->GetCompositor()->ScheduleFullRedraw();
#elif defined(USE_AURA)
  auto* compositor = static_cast<RenderWidgetHostViewAura*>(
                         web_contents->GetRenderWidgetHostView())
                         ->GetCompositor();
  compositor->ScheduleFullRedraw();
#endif
}

namespace {

// Helper to return a 200 OK non-cacheable response for a first request, and
// redirect the second request to the URL indicated in the query param.
std::unique_ptr<net::test_server::HttpResponse>
RedirectToTargetOnSecondNavigation(
    unsigned int& navigation_counter,
    const net::test_server::HttpRequest& request) {
  ++navigation_counter;
  if (navigation_counter == 1) {
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HttpStatusCode::HTTP_OK);
    http_response->AddCustomHeader("Cache-Control",
                                   "no-store, must-revalidate");
    return http_response;
  }

  std::string url_from_query =
      base::UnescapeBinaryURLComponent(request.GetURL().query_piece());
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HttpStatusCode::HTTP_FOUND);
  http_response->AddCustomHeader("Location", url_from_query);
  return http_response;
}

}  // namespace

void AddRedirectOnSecondNavigationHandler(net::EmbeddedTestServer* server) {
  unsigned int navigation_counter = 0;
  server->RegisterDefaultHandler(base::BindRepeating(
      &net::test_server::HandlePrefixedRequest,
      "/redirect-on-second-navigation",
      base::BindRepeating(&RedirectToTargetOnSecondNavigation,
                          base::OwnedRef(navigation_counter))));
}

}  // namespace content
