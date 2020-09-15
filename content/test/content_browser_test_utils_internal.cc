// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/content_browser_test_utils_internal.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/stack.h"
#include "base/json/json_reader.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/renderer_host/delegated_frame_host.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_javascript_dialog_manager.h"
#include "third_party/blink/public/common/frame/frame_visual_properties.h"

namespace content {

bool NavigateFrameToURL(FrameTreeNode* node, const GURL& url) {
  TestFrameNavigationObserver observer(node);
  NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PAGE_TRANSITION_LINK;
  params.frame_tree_node_id = node->frame_tree_node_id();
  node->navigator().GetController()->LoadURLWithParams(params);
  observer.Wait();

  if (!observer.last_navigation_succeeded()) {
    DLOG(WARNING) << "Navigation did not succeed: " << url;
    return false;
  }
  if (url != node->current_url()) {
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

FrameTreeVisualizer::FrameTreeVisualizer() {
}

FrameTreeVisualizer::~FrameTreeVisualizer() {
}

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

  // Traversal 3: Assign names to the proxies and add them to |legend| too.
  // Typically, only openers should have their names assigned this way.
  for (to_explore.push(root); !to_explore.empty();) {
    FrameTreeNode* node = to_explore.top();
    to_explore.pop();
    for (size_t i = node->child_count(); i-- != 0;) {
      to_explore.push(node->child_at(i));
    }

    // Sort the proxies by SiteInstance ID to avoid unordered_map ordering.
    std::vector<SiteInstance*> site_instances;
    for (const auto& proxy_pair :
         node->render_manager()->GetAllProxyHostsForTesting()) {
      site_instances.push_back(proxy_pair.second->GetSiteInstance());
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
      for (FrameTreeNode* up = node->parent()->frame_tree_node(); up != root;
           up = FrameTreeNode::From(up->parent())) {
        if (up->parent()->child_at(up->parent()->child_count() - 1) != up)
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
            GetName(proxy_pair.second->GetSiteInstance()));
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
    std::string description = site_instance->GetSiteURL().spec();
    base::StringAppendF(&result, "\n%s%s = %s", prefix,
                        legend_entry.first.c_str(), description.c_str());
    // Highlight some exceptionable conditions.
    if (site_instance->active_frame_count() == 0)
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
      std::find(seen_site_instance_ids_.begin(), seen_site_instance_ids_.end(),
                site_instance->GetId()) -
      seen_site_instance_ids_.begin();
  if (index == seen_site_instance_ids_.size())
    seen_site_instance_ids_.push_back(site_instance->GetId());

  // Whosoever writes a test using >=26 site instances shall be a lucky ducky.
  if (index < 25)
    return base::StringPrintf("%c", 'A' + static_cast<char>(index));
  else
    return base::StringPrintf("Z%d", static_cast<int>(index - 25));
}

Shell* OpenPopup(const ToRenderFrameHost& opener,
                 const GURL& url,
                 const std::string& name) {
  TestNavigationObserver observer(url);
  observer.StartWatchingNewWebContents();

  ShellAddedObserver new_shell_observer;
  bool did_create_popup = false;
  bool did_execute_script = ExecuteScriptAndExtractBool(
      opener,
      "window.domAutomationController.send("
      "    !!window.open('" + url.spec() + "', '" + name + "'));",
      &did_create_popup);
  if (!did_execute_script || !did_create_popup)
    return nullptr;

  observer.Wait();

  Shell* new_shell = new_shell_observer.GetShell();
  EXPECT_EQ(url,
            new_shell->web_contents()->GetMainFrame()->GetLastCommittedURL());
  return new_shell_observer.GetShell();
}

FileChooserDelegate::FileChooserDelegate(const base::FilePath& file,
                                         base::OnceClosure callback)
    : file_(file), callback_(std::move(callback)) {}

FileChooserDelegate::~FileChooserDelegate() = default;

void FileChooserDelegate::RunFileChooser(
    RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  // Send the selected file to the renderer process.
  auto file_info = blink::mojom::FileChooserFileInfo::NewNativeFile(
      blink::mojom::NativeFileInfo::New(file_, base::string16()));
  std::vector<blink::mojom::FileChooserFileInfoPtr> files;
  files.push_back(std::move(file_info));
  listener->FileSelected(std::move(files), base::FilePath(),
                         blink::mojom::FileChooserParams::Mode::kOpen);

  params_ = params.Clone();
  std::move(callback_).Run();
}

FrameTestNavigationManager::FrameTestNavigationManager(
    int filtering_frame_tree_node_id,
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
    : content::WebContentsObserver(frame_tree_node->current_frame_host()
                                       ->delegate()
                                       ->GetAsWebContents()),
      frame_tree_node_id_(frame_tree_node->frame_tree_node_id()),
      url_(url) {
}

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

base::Optional<bad_message::BadMessageReason>
RenderProcessHostBadIpcMessageWaiter::Wait() {
  base::Optional<int> internal_result = internal_waiter_.Wait();
  if (!internal_result.has_value())
    return base::nullopt;
  return static_cast<bad_message::BadMessageReason>(internal_result.value());
}

ShowWidgetMessageFilter::ShowWidgetMessageFilter(WebContents* web_contents)
#if defined(OS_MAC) || defined(OS_ANDROID)
    : BrowserMessageFilter(FrameMsgStart),
#else
    : BrowserMessageFilter(ViewMsgStart),
#endif
      run_loop_(std::make_unique<base::RunLoop>()) {
  WebContentsObserver::Observe(web_contents);
}

ShowWidgetMessageFilter::~ShowWidgetMessageFilter() {
  DCHECK(is_shut_down_);
}

void ShowWidgetMessageFilter::Shutdown() {
  WebContentsObserver::Observe(nullptr);
  is_shut_down_ = true;
}

bool ShowWidgetMessageFilter::OnMessageReceived(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(ShowWidgetMessageFilter, message)
#if !defined(OS_MAC) && !defined(OS_ANDROID)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowWidget, OnShowWidget)
#endif
  IPC_END_MESSAGE_MAP()
  return false;
}

void ShowWidgetMessageFilter::Wait() {
  DCHECK(!is_shut_down_);
  run_loop_->Run();
}

void ShowWidgetMessageFilter::Reset() {
  DCHECK(!is_shut_down_);
  initial_rect_ = gfx::Rect();
  routing_id_ = MSG_ROUTING_NONE;
  run_loop_ = std::make_unique<base::RunLoop>();
}

void ShowWidgetMessageFilter::OnShowWidget(int route_id,
                                           const gfx::Rect& initial_rect) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&ShowWidgetMessageFilter::OnShowWidgetOnUI,
                                this, route_id, initial_rect));
}

#if defined(OS_MAC) || defined(OS_ANDROID)
bool ShowWidgetMessageFilter::ShowPopupMenu(
    RenderFrameHost* render_frame_host,
    mojo::PendingRemote<blink::mojom::PopupMenuClient>* popup_client,
    const gfx::Rect& bounds,
    int32_t item_height,
    double font_size,
    int32_t selected_item,
    std::vector<blink::mojom::MenuItemPtr>* menu_items,
    bool right_aligned,
    bool allow_multiple_selection) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&ShowWidgetMessageFilter::OnShowWidgetOnUI,
                                this, MSG_ROUTING_NONE, bounds));
  return true;
}
#endif

void ShowWidgetMessageFilter::OnShowWidgetOnUI(int route_id,
                                               const gfx::Rect& initial_rect) {
  initial_rect_ = initial_rect;
  routing_id_ = route_id;
  run_loop_->Quit();
}

DropMessageFilter::DropMessageFilter(uint32_t message_class,
                                     uint32_t drop_message_id)
    : BrowserMessageFilter(message_class), drop_message_id_(drop_message_id) {}

DropMessageFilter::~DropMessageFilter() = default;

bool DropMessageFilter::OnMessageReceived(const IPC::Message& message) {
  return message.type() == drop_message_id_;
}

ObserveMessageFilter::ObserveMessageFilter(uint32_t message_class,
                                           uint32_t watch_message_id)
    : BrowserMessageFilter(message_class),
      watch_message_id_(watch_message_id) {}

ObserveMessageFilter::~ObserveMessageFilter() = default;

void ObserveMessageFilter::Wait() {
  base::RunLoop loop;
  quit_closure_ = loop.QuitClosure();
  loop.Run();
}

bool ObserveMessageFilter::OnMessageReceived(const IPC::Message& message) {
  if (message.type() == watch_message_id_) {
    // Exit the Wait() method if it's being used, but in a fresh stack once the
    // message is actually handled.
    if (quit_closure_ && !received_) {
      base::ThreadPool::PostTask(
          FROM_HERE, base::BindOnce(&ObserveMessageFilter::QuitWait, this));
    }
    received_ = true;
  }
  return false;
}

void ObserveMessageFilter::QuitWait() {
  std::move(quit_closure_).Run();
}

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
    std::move(callback_).Run(true, base::string16());

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

void BeforeUnloadBlockingDelegate::RunJavaScriptDialog(
    WebContents* web_contents,
    RenderFrameHost* render_frame_host,
    JavaScriptDialogType dialog_type,
    const base::string16& message_text,
    const base::string16& default_prompt_text,
    DialogClosedCallback callback,
    bool* did_suppress_message) {
  NOTREACHED();
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
    const base::string16* prompt_override) {
  NOTREACHED();
  return true;
}

namespace {
static constexpr int kEnableLogMessageId = 0;
static constexpr char kEnableLogMessage[] = R"({"id":0,"method":"Log.enable"})";
static constexpr int kDisableLogMessageId = 1;
static constexpr char kDisableLogMessage[] =
    R"({"id":1,"method":"Log.disable"})";
}  // namespace

DevToolsInspectorLogWatcher::DevToolsInspectorLogWatcher(
    WebContents* web_contents) {
  host_ = DevToolsAgentHost::GetOrCreateFor(web_contents);
  host_->AttachClient(this);

  host_->DispatchProtocolMessage(
      this, base::as_bytes(
                base::make_span(kEnableLogMessage, strlen(kEnableLogMessage))));

  run_loop_enable_log_.Run();
}

DevToolsInspectorLogWatcher::~DevToolsInspectorLogWatcher() {
  host_->DetachClient(this);
}

void DevToolsInspectorLogWatcher::DispatchProtocolMessage(
    DevToolsAgentHost* host,
    base::span<const uint8_t> message) {
  base::StringPiece message_str(reinterpret_cast<const char*>(message.data()),
                                message.size());
  auto parsed_message = base::JSONReader::Read(message_str);
  base::Optional<int> command_id = parsed_message->FindIntPath("id");
  if (command_id.has_value()) {
    switch (command_id.value()) {
      case kEnableLogMessageId:
        run_loop_enable_log_.Quit();
        break;
      case kDisableLogMessageId:
        run_loop_disable_log_.Quit();
        break;
      default:
        NOTREACHED();
    }
    return;
  }

  std::string* notification = parsed_message->FindStringPath("method");
  if (notification && *notification == "Log.entryAdded") {
    std::string* text = parsed_message->FindStringPath("params.entry.text");
    DCHECK(text);
    last_message_ = *text;
  }
}

void DevToolsInspectorLogWatcher::AgentHostClosed(DevToolsAgentHost* host) {}

void DevToolsInspectorLogWatcher::FlushAndStopWatching() {
  host_->DispatchProtocolMessage(
      this, base::as_bytes(base::make_span(kDisableLogMessage,
                                           strlen(kDisableLogMessage))));
  run_loop_disable_log_.Run();
}

}  // namespace content
