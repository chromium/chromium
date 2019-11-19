// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_LOCAL_FRAME_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_LOCAL_FRAME_H_

#include <memory>
#include <set>

#include "base/callback.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/common/frame/sandbox_flags.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"
#include "third_party/blink/public/mojom/ad_tagging/ad_frame.mojom-shared.h"
#include "third_party/blink/public/mojom/commit_result/commit_result.mojom-shared.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-shared.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-shared.h"
#include "third_party/blink/public/mojom/portal/portal.mojom-shared.h"
#include "third_party/blink/public/mojom/selection_menu/selection_menu_behavior.mojom-shared.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-shared.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_focus_type.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/public/web/web_ime_text_span.h"
#include "third_party/blink/public/web/web_navigation_params.h"
#include "third_party/blink/public/web/web_text_direction.h"
#include "v8/include/v8.h"

namespace blink {

class FrameScheduler;
class InterfaceRegistry;
class WebAssociatedURLLoader;
class WebAutofillClient;
class WebContentCaptureClient;
class WebContentSettingsClient;
class WebDocument;
class WebDoubleSize;
class WebDOMMessageEvent;
class WebLocalFrameClient;
class WebFrameWidget;
class WebInputMethodController;
class WebPerformance;
class WebRange;
class WebSecurityOrigin;
class WebScriptExecutionCallback;
class WebSpellCheckPanelHostClient;
class WebString;
class WebTextCheckClient;
class WebURL;
class WebView;
enum class WebTreeScopeType;
struct TransferableMessage;
struct WebAssociatedURLLoaderOptions;
struct WebConsoleMessage;
struct WebContentSecurityPolicyViolation;
struct WebIsolatedWorldInfo;
struct MediaPlayerAction;
struct WebPoint;
struct WebPrintParams;
struct WebPrintPresetOptions;
struct WebScriptSource;
struct WebSourceLocation;

// Interface for interacting with in process frames. This contains methods that
// require interacting with a frame's document.
// FIXME: Move lots of methods from WebFrame in here.
class WebLocalFrame : public WebFrame {
 public:
  // Creates a main local frame for the WebView. Can only be invoked when no
  // main frame exists yet. Call Close() to release the returned frame.
  // WebLocalFrameClient may not be null.
  // TODO(dcheng): The argument order should be more consistent with
  // CreateLocalChild() and CreateRemoteChild() in WebRemoteFrame... but it's so
  // painful...
  BLINK_EXPORT static WebLocalFrame* CreateMainFrame(
      WebView*,
      WebLocalFrameClient*,
      blink::InterfaceRegistry*,
      WebFrame* opener = nullptr,
      const WebString& name = WebString(),
      WebSandboxFlags = WebSandboxFlags::kNone,
      const FeaturePolicy::FeatureState& opener_feature_state =
          FeaturePolicy::FeatureState());

  // Used to create a provisional local frame. Currently, it's possible for a
  // provisional navigation not to commit (i.e. it might turn into a download),
  // but this can only be determined by actually trying to load it. The loading
  // process depends on having a corresponding LocalFrame in Blink to hold all
  // the pending state.
  //
  // When a provisional frame is first created, it is only partially attached to
  // the frame tree. This means that though a provisional frame might have a
  // frame owner, the frame owner's content frame does not point back at the
  // provisional frame. Similarly, though a provisional frame may have a parent
  // frame pointer, the parent frame's children list will not contain the
  // provisional frame. Thus, a provisional frame is invisible to the rest of
  // Blink unless the navigation commits and the provisional frame is fully
  // attached to the frame tree by calling Swap(). It swaps with the
  // |previous_web_frame|.
  //
  // Otherwise, if the load should not commit, call Detach() to discard the
  // frame.
  BLINK_EXPORT static WebLocalFrame* CreateProvisional(
      WebLocalFrameClient*,
      blink::InterfaceRegistry*,
      WebFrame* previous_web_frame,
      const FramePolicy&);

  // Creates a new local child of this frame. Similar to the other methods that
  // create frames, the returned frame should be freed by calling Close() when
  // it's no longer needed.
  virtual WebLocalFrame* CreateLocalChild(WebTreeScopeType,
                                          WebLocalFrameClient*,
                                          blink::InterfaceRegistry*) = 0;

  // Returns the WebFrame associated with the current V8 context. This
  // function can return 0 if the context is associated with a Document that
  // is not currently being displayed in a Frame.
  BLINK_EXPORT static WebLocalFrame* FrameForCurrentContext();

  // Returns the frame corresponding to the given context. This can return 0
  // if the context is detached from the frame, or if the context doesn't
  // correspond to a frame (e.g., workers).
  BLINK_EXPORT static WebLocalFrame* FrameForContext(v8::Local<v8::Context>);

  // Returns the frame inside a given frame or iframe element. Returns 0 if
  // the given element is not a frame, iframe or if the frame is empty.
  BLINK_EXPORT static WebLocalFrame* FromFrameOwnerElement(const WebElement&);

  virtual WebLocalFrameClient* Client() const = 0;

  // Initialization ---------------------------------------------------------

  virtual void SetAutofillClient(WebAutofillClient*) = 0;
  virtual WebAutofillClient* AutofillClient() = 0;

  virtual void SetContentCaptureClient(WebContentCaptureClient*) = 0;
  virtual WebContentCaptureClient* ContentCaptureClient() const = 0;

  // Closing -------------------------------------------------------------

  // Runs unload handlers for this frame.
  virtual void DispatchUnloadEvent() = 0;

  // Basic properties ---------------------------------------------------

  // The urls of the given combination types of favicon (if any) specified by
  // the document loaded in this frame. The iconTypesMask is a bit-mask of
  // WebIconURL::Type values, used to select from the available set of icon
  // URLs
  virtual WebVector<WebIconURL> IconURLs(int icon_types_mask) const = 0;

  virtual WebDocument GetDocument() const = 0;

  // The name of this frame. If no name is given, empty string is returned.
  virtual WebString AssignedName() const = 0;

  // Sets the name of this frame.
  virtual void SetName(const WebString&) = 0;

  // Hierarchy ----------------------------------------------------------

  // Returns true if the current frame is a local root.
  virtual bool IsLocalRoot() const = 0;

  // Returns true if the current frame is a provisional frame.
  // TODO(https://crbug.com/578349): provisional frames are a hack that should
  // be removed.
  virtual bool IsProvisional() const = 0;

  // Get the highest-level LocalFrame in this frame's in-process subtree.
  virtual WebLocalFrame* LocalRoot() = 0;

  // Returns the WebFrameWidget associated with this frame if there is one or
  // nullptr otherwise.
  // TODO(dcheng): The behavior of this will be changing to always return a
  // WebFrameWidget. Use IsLocalRoot() if it's important to tell if a frame is a
  // local root.
  virtual WebFrameWidget* FrameWidget() const = 0;

  // Returns the frame identified by the given name.  This method supports
  // pseudo-names like _self, _top, and _blank and otherwise performs the same
  // kind of lookup what |window.open(..., name)| would in Javascript.
  virtual WebFrame* FindFrameByName(const WebString& name) = 0;

  // Starts scrolling to a specific offset in a frame. Returns false on failure.
  virtual bool ScrollTo(const gfx::Point& scrollPosition,
                        bool animate,
                        base::OnceClosure on_finish) = 0;

  // Navigation Ping --------------------------------------------------------

  virtual void SendPings(const WebURL& destination_url) = 0;

  // Navigation ----------------------------------------------------------

  // Start reloading the current document.
  // Note: StartReload() will be deprecated, use StartNavigation() instead.
  virtual void StartReload(WebFrameLoadType) = 0;

  // Start navigation to the given URL.
  virtual void StartNavigation(const WebURLRequest&) = 0;

  // View-source rendering mode.  Set this before loading an URL to cause
  // it to be rendered in view-source mode.
  virtual void EnableViewSourceMode(bool) = 0;
  virtual bool IsViewSourceModeEnabled() const = 0;

  // Returns the document loader that is currently loaded.
  virtual WebDocumentLoader* GetDocumentLoader() const = 0;

  // Called when a navigation is blocked because a Content Security Policy (CSP)
  // is infringed.
  virtual void ReportContentSecurityPolicyViolation(
      const blink::WebContentSecurityPolicyViolation&) = 0;

  // Sets the referrer for the given request to be the specified URL or
  // if that is null, then it sets the referrer to the referrer that the
  // frame would use for subresources.  NOTE: This method also filters
  // out invalid referrers (e.g., it is invalid to send a HTTPS URL as
  // the referrer for a HTTP request).
  virtual void SetReferrerForRequest(WebURLRequest&, const WebURL&) = 0;

  // Navigation State -------------------------------------------------------

  // Returns true if there is a pending redirect or location change
  // within specified interval. This could be caused by:
  // * an HTTP Refresh header
  // * an X-Frame-Options header
  // * the respective http-equiv meta tags
  // * window.location value being mutated
  // * CSP policy block
  // * reload
  // * form submission
  virtual bool IsNavigationScheduledWithin(base::TimeDelta interval) const = 0;

  // Reports a list of Blink features used, performed or encountered by the
  // browser during the current page load happening on the frame.
  virtual void BlinkFeatureUsageReport(
      const std::set<blink::mojom::WebFeature>& features) = 0;
  virtual void BlinkFeatureUsageReport(blink::mojom::WebFeature feature) = 0;

  // Informs the renderer that mixed content was found externally regarding this
  // frame. Currently only the the browser process can do so. The included data
  // is used for instance to report to the CSP policy and to log to the frame
  // console.
  virtual void MixedContentFound(const WebURL& main_resource_url,
                                 const WebURL& mixed_content_url,
                                 mojom::RequestContextType,
                                 bool was_allowed,
                                 bool had_redirect,
                                 const WebSourceLocation&) = 0;

  // Orientation Changes ----------------------------------------------------

  // Notify the frame that the screen orientation has changed.
  virtual void SendOrientationChangeEvent() = 0;

  // CSS3 Paged Media ----------------------------------------------------

  // Returns true if page box (margin boxes and page borders) is visible.
  virtual bool IsPageBoxVisible(int page_index) = 0;

  // Returns true if the page style has custom size information.
  virtual bool HasCustomPageSizeStyle(int page_index) = 0;

  // Returns the preferred page size and margins in pixels, assuming 96
  // pixels per inch. pageSize, marginTop, marginRight, marginBottom,
  // marginLeft must be initialized to the default values that are used if
  // auto is specified.
  virtual void PageSizeAndMarginsInPixels(int page_index,
                                          WebDoubleSize& page_size,
                                          int& margin_top,
                                          int& margin_right,
                                          int& margin_bottom,
                                          int& margin_left) = 0;

  // Returns the value for a page property that is only defined when printing.
  // printBegin must have been called before this method.
  virtual WebString PageProperty(const WebString& property_name,
                                 int page_index) = 0;

  // Scripting --------------------------------------------------------------

  // Executes script in the context of the current page.
  virtual void ExecuteScript(const WebScriptSource&) = 0;

  // Executes JavaScript in a new world associated with the web frame.
  // The script gets its own global scope and its own prototypes for
  // intrinsic JavaScript objects (String, Array, and so-on). It also
  // gets its own wrappers for all DOM nodes and DOM constructors.
  //
  // worldID must be > 0 (as 0 represents the main world).
  // worldID must be < kEmbedderWorldIdLimit, high number used internally.
  virtual void ExecuteScriptInIsolatedWorld(int32_t world_id,
                                            const WebScriptSource&) = 0;

  // worldID must be > 0 (as 0 represents the main world).
  // worldID must be < kEmbedderWorldIdLimit, high number used internally.
  // DEPRECATED: Use WebLocalFrame::requestExecuteScriptInIsolatedWorld.
  WARN_UNUSED_RESULT virtual v8::Local<v8::Value>
  ExecuteScriptInIsolatedWorldAndReturnValue(int32_t world_id,
                                             const WebScriptSource&) = 0;

  // Clears the isolated world CSP stored for |world_id| by this frame's
  // Document.
  virtual void ClearIsolatedWorldCSPForTesting(int32_t world_id) = 0;

  // Sets up an isolated world by associating a |world_id| with |info|.
  // worldID must be > 0 (as 0 represents the main world).
  // worldID must be < kEmbedderWorldIdLimit, high number used internally.
  virtual void SetIsolatedWorldInfo(int32_t world_id,
                                    const WebIsolatedWorldInfo& info) = 0;

  // Executes script in the context of the current page and returns the value
  // that the script evaluated to.
  // DEPRECATED: Use WebLocalFrame::requestExecuteScriptAndReturnValue.
  virtual v8::Local<v8::Value> ExecuteScriptAndReturnValue(
      const WebScriptSource&) = 0;

  // Call the function with the given receiver and arguments, bypassing
  // canExecute().
  virtual v8::MaybeLocal<v8::Value> CallFunctionEvenIfScriptDisabled(
      v8::Local<v8::Function>,
      v8::Local<v8::Value>,
      int argc,
      v8::Local<v8::Value> argv[]) = 0;

  // Returns the V8 context for associated with the main world and this
  // frame. There can be many V8 contexts associated with this frame, one for
  // each isolated world and one for the main world. If you don't know what
  // the "main world" or an "isolated world" is, then you probably shouldn't
  // be calling this API.
  virtual v8::Local<v8::Context> MainWorldScriptContext() const = 0;

  // Executes script in the context of the current page and returns the value
  // that the script evaluated to with callback. Script execution can be
  // suspend.
  // DEPRECATED: Prefer requestExecuteScriptInIsolatedWorld().
  virtual void RequestExecuteScriptAndReturnValue(
      const WebScriptSource&,
      bool user_gesture,
      WebScriptExecutionCallback*) = 0;

  // Requests execution of the given function, but allowing for script
  // suspension and asynchronous execution.
  virtual void RequestExecuteV8Function(v8::Local<v8::Context>,
                                        v8::Local<v8::Function>,
                                        v8::Local<v8::Value> receiver,
                                        int argc,
                                        v8::Local<v8::Value> argv[],
                                        WebScriptExecutionCallback*) = 0;

  enum ScriptExecutionType {
    // Execute script synchronously, unless the page is suspended.
    kSynchronous,
    // Execute script asynchronously.
    kAsynchronous,
    // Execute script asynchronously, blocking the window.onload event.
    kAsynchronousBlockingOnload
  };

  // worldID must be > 0 (as 0 represents the main world).
  // worldID must be < kEmbedderWorldIdLimit, high number used internally.
  virtual void RequestExecuteScriptInIsolatedWorld(
      int32_t world_id,
      const WebScriptSource* source_in,
      unsigned num_sources,
      bool user_gesture,
      ScriptExecutionType,
      WebScriptExecutionCallback*) = 0;

  // Logs to the console associated with this frame. If |discard_duplicates| is
  // set, the message will only be added if it is unique (i.e. has not been
  // added to the console previously from this page).
  void AddMessageToConsole(const WebConsoleMessage& message,
                           bool discard_duplicates = false) {
    AddMessageToConsoleImpl(message, discard_duplicates);
  }

  // Expose modal dialog methods to avoid having to go through JavaScript.
  virtual void Alert(const WebString& message) = 0;
  virtual bool Confirm(const WebString& message) = 0;
  virtual WebString Prompt(const WebString& message,
                           const WebString& default_value) = 0;

  // Debugging -----------------------------------------------------------

  virtual void BindDevToolsAgent(
      mojo::ScopedInterfaceEndpointHandle devtools_agent_host_ptr_info,
      mojo::ScopedInterfaceEndpointHandle devtools_agent_request) = 0;

  // Editing -------------------------------------------------------------

  virtual void SetMarkedText(const WebString& text,
                             unsigned location,
                             unsigned length) = 0;
  virtual void UnmarkText() = 0;
  virtual bool HasMarkedText() const = 0;

  virtual WebRange MarkedRange() const = 0;

  // Returns the text range rectangle in the viepwort coordinate space.
  virtual bool FirstRectForCharacterRange(unsigned location,
                                          unsigned length,
                                          WebRect&) const = 0;

  // Returns the index of a character in the Frame's text stream at the given
  // point. The point is in the viewport coordinate space. Will return
  // WTF::notFound if the point is invalid.
  virtual size_t CharacterIndexForPoint(const WebPoint&) const = 0;

  // Supports commands like Undo, Redo, Cut, Copy, Paste, SelectAll,
  // Unselect, etc. See EditorCommand.cpp for the full list of supported
  // commands.
  virtual bool ExecuteCommand(const WebString&) = 0;
  virtual bool ExecuteCommand(const WebString&, const WebString& value) = 0;
  virtual bool IsCommandEnabled(const WebString&) const = 0;

  // Returns the text direction at the start and end bounds of the current
  // selection.  If the selection range is empty, it returns false.
  virtual bool SelectionTextDirection(WebTextDirection& start,
                                      WebTextDirection& end) const = 0;
  // Returns true if the selection range is nonempty and its anchor is first
  // (i.e its anchor is its start).
  virtual bool IsSelectionAnchorFirst() const = 0;
  // Changes the text direction of the selected input node.
  virtual void SetTextDirection(WebTextDirection) = 0;

  // Selection -----------------------------------------------------------

  virtual bool HasSelection() const = 0;

  virtual WebRange SelectionRange() const = 0;

  virtual WebString SelectionAsText() const = 0;
  virtual WebString SelectionAsMarkup() const = 0;

  // Expands the selection to a word around the caret and returns
  // true. Does nothing and returns false if there is no caret or
  // there is ranged selection.
  virtual bool SelectWordAroundCaret() = 0;

  // DEPRECATED: Use moveRangeSelection.
  virtual void SelectRange(const WebPoint& base, const WebPoint& extent) = 0;

  enum HandleVisibilityBehavior {
    // Hide handle(s) in the new selection.
    kHideSelectionHandle,
    // Show handle(s) in the new selection.
    kShowSelectionHandle,
    // Keep the current handle visibility.
    kPreserveHandleVisibility,
  };

  virtual void SelectRange(const WebRange&,
                           HandleVisibilityBehavior,
                           mojom::SelectionMenuBehavior) = 0;

  virtual WebString RangeAsText(const WebRange&) = 0;

  // Move the current selection to the provided viewport point/points. If the
  // current selection is editable, the new selection will be restricted to
  // the root editable element.
  // |TextGranularity| represents character wrapping granularity. If
  // WordGranularity is set, WebFrame extends selection to wrap word.
  virtual void MoveRangeSelection(
      const WebPoint& base,
      const WebPoint& extent,
      WebFrame::TextGranularity = kCharacterGranularity) = 0;
  virtual void MoveCaretSelection(const WebPoint&) = 0;

  virtual bool SetEditableSelectionOffsets(int start, int end) = 0;
  virtual bool SetCompositionFromExistingText(
      int composition_start,
      int composition_end,
      const WebVector<WebImeTextSpan>& ime_text_spans) = 0;
  virtual void ExtendSelectionAndDelete(int before, int after) = 0;

  virtual void SetCaretVisible(bool) = 0;

  // Moves the selection extent point. This function does not allow the
  // selection to collapse. If the new extent is set to the same position as
  // the current base, this function will do nothing.
  virtual void MoveRangeSelectionExtent(const WebPoint&) = 0;
  // Replaces the selection with the input string.
  virtual void ReplaceSelection(const WebString&) = 0;
  // Deletes text before and after the current cursor position, excluding the
  // selection. The lengths are supplied in UTF-16 Code Unit, not in code points
  // or in glyphs.
  virtual void DeleteSurroundingText(int before, int after) = 0;
  // A variant of deleteSurroundingText(int, int). Major differences are:
  // 1. The lengths are supplied in code points, not in UTF-16 Code Unit or in
  // glyphs.
  // 2. This method does nothing if there are one or more invalid surrogate
  // pairs in the requested range.
  virtual void DeleteSurroundingTextInCodePoints(int before, int after) = 0;

  virtual void ExtractSmartClipData(WebRect rect_in_viewport,
                                    WebString& clip_text,
                                    WebString& clip_html,
                                    WebRect& clip_rect) = 0;

  // Spell-checking support -------------------------------------------------
  virtual void SetTextCheckClient(WebTextCheckClient*) = 0;
  virtual void SetSpellCheckPanelHostClient(WebSpellCheckPanelHostClient*) = 0;
  virtual WebSpellCheckPanelHostClient* SpellCheckPanelHostClient() const = 0;
  virtual void ReplaceMisspelledRange(const WebString&) = 0;
  virtual void RemoveSpellingMarkers() = 0;
  virtual void RemoveSpellingMarkersUnderWords(
      const WebVector<WebString>& words) = 0;

  // Content Settings -------------------------------------------------------

  virtual void SetContentSettingsClient(WebContentSettingsClient*) = 0;

  // Image reload -----------------------------------------------------------

  // If the provided node is an image that failed to load, reload it.
  virtual void ReloadImage(const WebNode&) = 0;

  // Feature usage logging --------------------------------------------------

  virtual void DidCallAddSearchProvider() = 0;
  virtual void DidCallIsSearchProviderInstalled() = 0;

  // Iframe sandbox ---------------------------------------------------------

  // TODO(ekaramad): This method is only exposed for testing for certain tests
  // outside of blink/ that are interested in approximate value of the
  // FrameReplicationState. This method should be replaced with one in content/
  // where the notion of FrameReplicationState is relevant to.
  // Returns the effective sandbox flags which are inherited from their parent
  // frame.
  virtual WebSandboxFlags EffectiveSandboxFlagsForTesting() const = 0;

  // Returns false if this frame, or any parent frame is sandboxed and does not
  // have the flag "allow-downloads-without-user-activation" set.
  virtual bool IsAllowedToDownloadWithoutUserActivation() const = 0;

  // Find-in-page -----------------------------------------------------------

  // Searches a frame for a given string. Only used for testing.
  //
  // If a match is found, this function will select it (scrolling down to
  // make it visible if needed) and fill in selectionRect with the
  // location of where the match was found (in window coordinates).
  //
  // If no match is found, this function clears all tickmarks and
  // highlighting.
  //
  // Returns true if the search string was found, false otherwise.
  virtual bool FindForTesting(int identifier,
                              const WebString& search_text,
                              bool match_case,
                              bool forward,
                              bool find_next,
                              bool force,
                              bool wrap_within_frame) = 0;

  // Set the tickmarks for the frame. This will override the default tickmarks
  // generated by find results. If this is called with an empty array, the
  // default behavior will be restored.
  virtual void SetTickmarks(const WebVector<WebRect>&) = 0;

  // Context menu -----------------------------------------------------------

  // Returns the node that the context menu opened over.
  virtual WebNode ContextMenuNode() const = 0;

  // Copy to the clipboard the image located at a particular point in visual
  // viewport coordinates.
  virtual void CopyImageAt(const WebPoint&) = 0;

  // Save as the image located at a particular point in visual viewport
  // coordinates.
  virtual void SaveImageAt(const WebPoint&) = 0;

  // Events --------------------------------------------------------------

  // Dispatches a message event on the current DOMWindow in this WebFrame.
  virtual void DispatchMessageEventWithOriginCheck(
      const WebSecurityOrigin& intended_target_origin,
      const WebDOMMessageEvent&) = 0;

  // TEMP: Usage count for chrome.loadtimes deprecation.
  // This will be removed following the deprecation.
  virtual void UsageCountChromeLoadTimes(const WebString& metric) = 0;

  // Portals -------------------------------------------------------------

  // Dispatches an event when a Portal gets activated. |portal_token| is the
  // portal's unique identifier, the message pipe |portal_pipe| is the
  // portal's mojo interface, and the message pipe |portal_client_pipe| is
  // a mojo interface to communicate back with the caller of the portal's
  // mojo interface. |data| is an optional message sent together with the
  // portal's activation.
  using OnPortalActivatedCallback =
      base::OnceCallback<void(mojom::PortalActivateResult)>;
  virtual void OnPortalActivated(
      const base::UnguessableToken& portal_token,
      mojo::ScopedInterfaceEndpointHandle portal_pipe,
      mojo::ScopedInterfaceEndpointHandle portal_client_pipe,
      TransferableMessage data,
      OnPortalActivatedCallback callback) = 0;

  // Forwards message to the PortalHost object exposed by the frame.
  virtual void ForwardMessageFromHost(
      TransferableMessage message,
      const WebSecurityOrigin& source_origin,
      const base::Optional<WebSecurityOrigin>& target_origin) = 0;

  // Scheduling ---------------------------------------------------------------

  virtual FrameScheduler* Scheduler() const = 0;

  // Task queues --------------------------------------------------------------

  // Returns frame-specific task runner to run tasks of this type on.
  // They have the same lifetime as the frame.
  virtual scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(
      TaskType) = 0;

  // Returns the WebInputMethodController associated with this local frame.
  virtual WebInputMethodController* GetInputMethodController() = 0;

  // Loading ------------------------------------------------------------------

  // Returns an AssociatedURLLoader that is associated with this frame.  The
  // loader will, for example, be cancelled when WebFrame::stopLoading is
  // called.
  //
  // FIXME: stopLoading does not yet cancel an associated loader!!
  virtual WebAssociatedURLLoader* CreateAssociatedURLLoader(
      const WebAssociatedURLLoaderOptions&) = 0;

  // Geometry -----------------------------------------------------------------

  // NOTE: These routines do not force page layout so their results may
  // not be accurate if the page layout is out-of-date.

  // The scroll offset from the top-left corner of the frame in pixels.
  virtual WebSize GetScrollOffset() const = 0;
  virtual void SetScrollOffset(const WebSize&) = 0;

  // The size of the document in this frame.
  virtual WebSize DocumentSize() const = 0;

  // Returns true if the contents (minus scrollbars) has non-zero area.
  virtual bool HasVisibleContent() const = 0;

  // Returns the visible content rect (minus scrollbars), relative to the
  // document.
  virtual WebRect VisibleContentRect() const = 0;

  // Printing ------------------------------------------------------------

  // Dispatch |beforeprint| event, and execute event handlers. They might detach
  // this frame from the owner WebView.
  // This function should be called before pairs of PrintBegin() and PrintEnd().
  virtual void DispatchBeforePrintEvent() = 0;

  // Reformats the WebFrame for printing. WebPrintParams specifies the printable
  // content size, paper size, printable area size, printer DPI and print
  // scaling option. If constrainToNode node is specified, then only the given
  // node is printed (for now only plugins are supported), instead of the entire
  // frame.
  // Returns the number of pages that can be printed at the given
  // page size.
  virtual int PrintBegin(const WebPrintParams&,
                         const WebNode& constrain_to_node = WebNode()) = 0;

  // Returns the page shrinking factor calculated by webkit (usually
  // between 1/1.33 and 1/2). Returns 0 if the page number is invalid or
  // not in printing mode.
  virtual float GetPrintPageShrink(int page) = 0;

  // Prints one page, and returns the calculated page shrinking factor
  // (usually between 1/1.33 and 1/2).  Returns 0 if the page number is
  // invalid or not in printing mode.
  virtual float PrintPage(int page_to_print, cc::PaintCanvas*) = 0;

  // Reformats the WebFrame for screen display.
  virtual void PrintEnd() = 0;

  // Dispatch |afterprint| event, and execute event handlers. They might detach
  // this frame from the owner WebView.
  // This function should be called after pairs of PrintBegin() and PrintEnd().
  virtual void DispatchAfterPrintEvent() = 0;

  // Returns true on success and sets the out parameter to the print preset
  // options for the document.
  virtual bool GetPrintPresetOptionsForPlugin(const WebNode&,
                                              WebPrintPresetOptions*) = 0;

  // Paint Preview ------------------------------------------------------------

  // Captures a full frame paint preview of the WebFrame including subframes.
  virtual bool CapturePaintPreview(const WebRect& bounds,
                                   cc::PaintCanvas* canvas) = 0;

  // Focus --------------------------------------------------------------

  // Advance the focus of the WebView to next text input element from current
  // input field wrt sequential navigation with TAB or Shift + TAB
  // WebFocusTypeForward simulates TAB and WebFocusTypeBackward simulates
  // Shift + TAB. (Will be extended to other form controls like select element,
  // checkbox, radio etc.)
  virtual void AdvanceFocusInForm(WebFocusType) = 0;

  // Returns whether the keyboard should be suppressed for the currently focused
  // element.
  virtual bool ShouldSuppressKeyboardForFocusedElement() = 0;

  // Performance --------------------------------------------------------

  virtual WebPerformance Performance() const = 0;

  // Ad Tagging ---------------------------------------------------------

  // True if the frame is thought (heuristically) to be created for
  // advertising purposes.
  virtual bool IsAdSubframe() const = 0;

  // This setter is available in case the embedder has more information about
  // whether or not the frame is an ad.
  virtual void SetIsAdSubframe(blink::mojom::AdFrameType ad_frame_type) = 0;

  // Testing ------------------------------------------------------------------

  // Prints the frame into the canvas, with page boundaries drawn as one pixel
  // wide blue lines. This method exists to support web tests.
  virtual void PrintPagesForTesting(cc::PaintCanvas*, const WebSize&) = 0;

  // Returns the bounds rect for current selection. If selection is performed
  // on transformed text, the rect will still bound the selection but will
  // not be transformed itself. If no selection is present, the rect will be
  // empty ((0,0), (0,0)).
  virtual WebRect GetSelectionBoundsRectForTesting() const = 0;

  // Performs the specified media player action on the media element at the
  // given location.
  virtual void PerformMediaPlayerAction(const WebPoint&,
                                        const MediaPlayerAction&) = 0;

  virtual void SetLifecycleState(mojom::FrameLifecycleState state) = 0;

  virtual void WasHidden() = 0;
  virtual void WasShown() = 0;

  // Grants ability to lookup a named frame via the FindFrame
  // WebLocalFrameClient API. Enhanced binding security checks that check the
  // agent cluster will be enabled for windows that do not have this permission.
  // This should only be used for extensions and the webview tag.
  virtual void SetAllowsCrossBrowsingInstanceFrameLookup() = 0;

 protected:
  explicit WebLocalFrame(WebTreeScopeType scope) : WebFrame(scope) {}

  // Inherited from WebFrame, but intentionally hidden: it never makes sense
  // to directly call these on a WebLocalFrame.
  bool IsWebLocalFrame() const override = 0;
  WebLocalFrame* ToWebLocalFrame() override = 0;
  bool IsWebRemoteFrame() const override = 0;
  WebRemoteFrame* ToWebRemoteFrame() override = 0;

  virtual void AddMessageToConsoleImpl(const WebConsoleMessage&,
                                       bool discard_duplicates) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_LOCAL_FRAME_H_
