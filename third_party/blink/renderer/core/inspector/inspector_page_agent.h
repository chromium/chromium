/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_PAGE_AGENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_PAGE_AGENT_H_

#include "base/macros.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_cache_options.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/inspector/inspector_base_agent.h"
#include "third_party/blink/renderer/core/inspector/protocol/Page.h"
#include "third_party/blink/renderer/core/loader/frame_loader_types.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8-inspector.h"

namespace blink {

namespace probe {
class RecalculateStyle;
class UpdateLayout;
}  // namespace probe

class Resource;
class Document;
class DocumentLoader;
class InspectedFrames;
class InspectorResourceContentLoader;
class LocalFrame;
class ScriptSourceCode;
enum class ResourceType : uint8_t;

using blink::protocol::Maybe;

class CORE_EXPORT InspectorPageAgent final
    : public InspectorBaseAgent<protocol::Page::Metainfo> {
 public:
  class Client {
   public:
    virtual ~Client() = default;
    virtual void PageLayoutInvalidated(bool resized) {}
    virtual void WaitForDebugger() {}
  };

  enum ResourceType {
    kDocumentResource,
    kStylesheetResource,
    kImageResource,
    kFontResource,
    kMediaResource,
    kScriptResource,
    kTextTrackResource,
    kXHRResource,
    kFetchResource,
    kEventSourceResource,
    kWebSocketResource,
    kManifestResource,
    kSignedExchangeResource,
    kOtherResource
  };

  static HeapVector<Member<Document>> ImportsForFrame(LocalFrame*);
  static bool CachedResourceContent(const Resource*,
                                    String* result,
                                    bool* base64_encoded);
  static bool SharedBufferContent(scoped_refptr<const SharedBuffer>,
                                  const String& mime_type,
                                  const String& text_encoding_name,
                                  String* result,
                                  bool* base64_encoded);

  static String ResourceTypeJson(ResourceType);
  static ResourceType ToResourceType(const blink::ResourceType);
  static String CachedResourceTypeJson(const Resource&);

  InspectorPageAgent(InspectedFrames*,
                     Client*,
                     InspectorResourceContentLoader*,
                     v8_inspector::V8InspectorSession*);

  // Page API for frontend
  protocol::Response enable() override;
  protocol::Response disable() override;
  protocol::Response addScriptToEvaluateOnLoad(const String& script_source,
                                               String* identifier) override;
  protocol::Response removeScriptToEvaluateOnLoad(
      const String& identifier) override;
  protocol::Response addScriptToEvaluateOnNewDocument(
      const String& source,
      Maybe<String> world_name,
      String* identifier) override;
  protocol::Response removeScriptToEvaluateOnNewDocument(
      const String& identifier) override;
  protocol::Response setLifecycleEventsEnabled(bool) override;
  protocol::Response reload(Maybe<bool> bypass_cache,
                            Maybe<String> script_to_evaluate_on_load) override;
  protocol::Response stopLoading() override;
  protocol::Response setAdBlockingEnabled(bool) override;
  protocol::Response getResourceTree(
      std::unique_ptr<protocol::Page::FrameResourceTree>* frame_tree) override;
  protocol::Response getFrameTree(
      std::unique_ptr<protocol::Page::FrameTree>*) override;
  void getResourceContent(const String& frame_id,
                          const String& url,
                          std::unique_ptr<GetResourceContentCallback>) override;
  void searchInResource(const String& frame_id,
                        const String& url,
                        const String& query,
                        Maybe<bool> case_sensitive,
                        Maybe<bool> is_regex,
                        std::unique_ptr<SearchInResourceCallback>) override;
  protocol::Response setDocumentContent(const String& frame_id,
                                        const String& html) override;
  protocol::Response setBypassCSP(bool enabled) override;

  protocol::Response startScreencast(Maybe<String> format,
                                     Maybe<int> quality,
                                     Maybe<int> max_width,
                                     Maybe<int> max_height,
                                     Maybe<int> every_nth_frame) override;
  protocol::Response stopScreencast() override;
  protocol::Response getLayoutMetrics(
      std::unique_ptr<protocol::Page::LayoutViewport>*,
      std::unique_ptr<protocol::Page::VisualViewport>*,
      std::unique_ptr<protocol::DOM::Rect>*) override;
  protocol::Response createIsolatedWorld(const String& frame_id,
                                         Maybe<String> world_name,
                                         Maybe<bool> grant_universal_access,
                                         int* execution_context_id) override;
  protocol::Response setFontFamilies(
      std::unique_ptr<protocol::Page::FontFamilies>) override;
  protocol::Response setFontSizes(
      std::unique_ptr<protocol::Page::FontSizes>) override;
  protocol::Response generateTestReport(const String& message,
                                        Maybe<String> group) override;

  protocol::Response setProduceCompilationCache(bool enabled) override;
  protocol::Response addCompilationCache(const String& url,
                                         const protocol::Binary& data) override;
  protocol::Response clearCompilationCache() override;
  protocol::Response waitForDebugger() override;
  protocol::Response setInterceptFileChooserDialog(bool enabled) override;

  // InspectorInstrumentation API
  void DidClearDocumentOfWindowObject(LocalFrame*);
  void DidNavigateWithinDocument(LocalFrame*);
  void DomContentLoadedEventFired(LocalFrame*);
  void LoadEventFired(LocalFrame*);
  void WillCommitLoad(LocalFrame*, DocumentLoader*);
  void FrameAttachedToParent(LocalFrame*);
  void FrameDetachedFromParent(LocalFrame*);
  void FrameStartedLoading(LocalFrame*);
  void FrameStoppedLoading(LocalFrame*);
  void FrameRequestedNavigation(Frame* target_frame,
                                const KURL&,
                                ClientNavigationReason,
                                NavigationPolicy);
  void FrameScheduledNavigation(LocalFrame*,
                                const KURL&,
                                base::TimeDelta delay,
                                ClientNavigationReason);
  void FrameClearedScheduledNavigation(LocalFrame*);
  void WillRunJavaScriptDialog();
  void DidRunJavaScriptDialog();
  void DidResizeMainFrame();
  void DidChangeViewport();
  void LifecycleEvent(LocalFrame*,
                      DocumentLoader*,
                      const char* name,
                      double timestamp);
  void PaintTiming(Document*, const char* name, double timestamp);
  void Will(const probe::UpdateLayout&);
  void Did(const probe::UpdateLayout&);
  void Will(const probe::RecalculateStyle&);
  void Did(const probe::RecalculateStyle&);
  void WindowOpen(Document*,
                  const String&,
                  const AtomicString&,
                  const WebWindowFeatures&,
                  bool);
  void ConsumeCompilationCache(const ScriptSourceCode& source,
                               v8::ScriptCompiler::CachedData**);
  void ProduceCompilationCache(const ScriptSourceCode& source,
                               v8::Local<v8::Script> script);
  void FileChooserOpened(LocalFrame* frame,
                         HTMLInputElement* element,
                         bool* intercepted);

  // Inspector Controller API
  void Restore() override;
  bool ScreencastEnabled();

  void Trace(Visitor*) const override;

 private:
  void GetResourceContentAfterResourcesContentLoaded(
      const String& frame_id,
      const String& url,
      std::unique_ptr<GetResourceContentCallback>);
  void SearchContentAfterResourcesContentLoaded(
      const String& frame_id,
      const String& url,
      const String& query,
      bool case_sensitive,
      bool is_regex,
      std::unique_ptr<SearchInResourceCallback>);
  scoped_refptr<DOMWrapperWorld> EnsureDOMWrapperWorld(
      LocalFrame* frame,
      const String& world_name,
      bool grant_universal_access);

  static KURL UrlWithoutFragment(const KURL&);

  void PageLayoutInvalidated(bool resized);

  std::unique_ptr<protocol::Page::Frame> BuildObjectForFrame(LocalFrame*);
  std::unique_ptr<protocol::Page::FrameTree> BuildObjectForFrameTree(
      LocalFrame*);
  std::unique_ptr<protocol::Page::FrameResourceTree> BuildObjectForResourceTree(
      LocalFrame*);
  Member<InspectedFrames> inspected_frames_;
  HashMap<String, protocol::Binary> compilation_cache_;
  using FrameIsolatedWorlds = HashMap<String, scoped_refptr<DOMWrapperWorld>>;
  HeapHashMap<WeakMember<LocalFrame>, FrameIsolatedWorlds> isolated_worlds_;
  v8_inspector::V8InspectorSession* v8_session_;
  Client* client_;
  String pending_script_to_evaluate_on_load_once_;
  String script_to_evaluate_on_load_once_;
  Member<InspectorResourceContentLoader> inspector_resource_content_loader_;
  bool intercept_file_chooser_ = false;
  int resource_content_loader_client_id_;
  InspectorAgentState::Boolean enabled_;
  InspectorAgentState::Boolean screencast_enabled_;
  InspectorAgentState::Boolean lifecycle_events_enabled_;
  InspectorAgentState::Boolean bypass_csp_enabled_;
  InspectorAgentState::StringMap scripts_to_evaluate_on_load_;
  InspectorAgentState::StringMap worlds_to_evaluate_on_load_;
  InspectorAgentState::String standard_font_family_;
  InspectorAgentState::String fixed_font_family_;
  InspectorAgentState::String serif_font_family_;
  InspectorAgentState::String sans_serif_font_family_;
  InspectorAgentState::String cursive_font_family_;
  InspectorAgentState::String fantasy_font_family_;
  InspectorAgentState::String pictograph_font_family_;
  InspectorAgentState::Integer standard_font_size_;
  InspectorAgentState::Integer fixed_font_size_;
  InspectorAgentState::Boolean produce_compilation_cache_;
  DISALLOW_COPY_AND_ASSIGN(InspectorPageAgent);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_PAGE_AGENT_H_
