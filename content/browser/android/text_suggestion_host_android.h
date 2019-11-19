// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_TEXT_SUGGESTION_HOST_ANDROID_H_
#define CONTENT_BROWSER_ANDROID_TEXT_SUGGESTION_HOST_ANDROID_H_

#include "content/browser/android/render_widget_host_connector.h"
#include "content/browser/renderer_host/input/timeout_monitor.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/input/input_host.mojom.h"
#include "third_party/blink/public/mojom/input/input_messages.mojom.h"

namespace content {

// This class, along with its Java counterpart TextSuggestionHost, is used to
// implement the Android text suggestion menu that appears when you tap a
// misspelled word. This class creates the Android implementation of
// mojom::TextSuggestionHost, which is used to communicate back-and-forth with
// Blink side code (these are separate classes due to lifecycle considerations;
// this class is created by ImeAdapterAndroid ctor and destroyed together with
// WebContents. Mojo code takes ownership of mojom::TextSuggestionHost).
class TextSuggestionHostAndroid : public RenderWidgetHostConnector,
                                  public WebContentsObserver {
 public:
  static void Create(JNIEnv* env, WebContents* web_contents);
  TextSuggestionHostAndroid(JNIEnv* env,
                            WebContents* web_contents);
  ~TextSuggestionHostAndroid() override;

  // RenderWidgetHostConnector implementation.
  void UpdateRenderProcessConnection(
      RenderWidgetHostViewAndroid* old_rwhva,
      RenderWidgetHostViewAndroid* new_rhwva) override;

  // Called from the Java text suggestion menu to have Blink apply a spell
  // check suggestion.
  void ApplySpellCheckSuggestion(
      JNIEnv*,
      const base::android::JavaParamRef<jobject>&,
      const base::android::JavaParamRef<jstring>& replacement);
  // Called from the Java text suggestion menu to have Blink apply a text
  // suggestion.
  void ApplyTextSuggestion(JNIEnv*,
                           const base::android::JavaParamRef<jobject>&,
                           int marker_tag,
                           int suggestion_index);
  // Called from the Java text suggestion menu to have Blink delete the
  // currently highlighted region of text that the open suggestion menu pertains
  // to.
  void DeleteActiveSuggestionRange(JNIEnv*,
                                   const base::android::JavaParamRef<jobject>&);
  // Called from the Java text suggestion menu to tell Blink that a word is
  // being added to the dictionary (so Blink can clear the spell check markers
  // for that word).
  void OnNewWordAddedToDictionary(
      JNIEnv*,
      const base::android::JavaParamRef<jobject>&,
      const base::android::JavaParamRef<jstring>& word);
  // Called from the Java text suggestion menu to tell Blink that the user
  // closed the menu without performing one of the available actions, so Blink
  // can re-show the insertion caret and remove the suggestion range highlight.
  void OnSuggestionMenuClosed(JNIEnv*,
                              const base::android::JavaParamRef<jobject>&);
  // Called from Blink to tell the Java TextSuggestionHost to open the spell
  // check suggestion menu.
  void ShowSpellCheckSuggestionMenu(
      double caret_x,
      double caret_y,
      const std::string& marked_text,
      const std::vector<blink::mojom::SpellCheckSuggestionPtr>& suggestions);
  // Called from Blink to tell the Java TextSuggestionHost to open the text
  // suggestion menu.
  void ShowTextSuggestionMenu(
      double caret_x,
      double caret_y,
      const std::string& marked_text,
      const std::vector<blink::mojom::TextSuggestionPtr>& suggestions);

  // Called by browser-side code in response to an input event to stop the
  // spell check menu timer and close the suggestion menu (if open).
  void OnKeyEvent();
  // Called by Blink when the user taps on a spell check marker and we might
  // want to show the text suggestion menu after the double-tap timer expires.
  void StartSuggestionMenuTimer();
  // Called by browser-side code in response to an input event to stop the
  // suggestion menu timer.
  void StopSuggestionMenuTimer();

 private:
  RenderFrameHost* GetFocusedFrame();
  base::android::ScopedJavaLocalRef<jobject> GetJavaTextSuggestionHost();
  const mojo::Remote<blink::mojom::TextSuggestionBackend>&
  GetTextSuggestionBackend();
  // Used by the spell check menu timer to notify Blink that the timer has
  // expired.
  void OnSuggestionMenuTimeout();
  double DpToPxIfNeeded(double value);

  // Current RenderWidgetHostView connected to this instance. Can be null.
  RenderWidgetHostViewAndroid* rwhva_;
  JavaObjectWeakGlobalRef java_text_suggestion_host_;
  mojo::Remote<blink::mojom::TextSuggestionBackend> text_suggestion_backend_;
  TimeoutMonitor suggestion_menu_timeout_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_TEXT_SUGGESTION_HOST_ANDROID_H_
