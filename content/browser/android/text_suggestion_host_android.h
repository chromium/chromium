// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_TEXT_SUGGESTION_HOST_ANDROID_H_
#define CONTENT_BROWSER_ANDROID_TEXT_SUGGESTION_HOST_ANDROID_H_

#include "base/memory/raw_ptr.h"
#include "components/input/timeout_monitor.h"
#include "content/public/browser/document_user_data.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/input/input_host.mojom.h"
#include "third_party/blink/public/mojom/input/input_messages.mojom.h"
#include "third_party/jni_zero/jni_zero.h"

namespace content {

class TextSuggestionHostMojoImplAndroid;

// This class, along with its Java counterpart TextSuggestionHost, is used to
// implement the Android text suggestion menu that appears when you tap a
// misspelled word. This class creates the Android implementation of
// mojom::TextSuggestionHost.
class TextSuggestionHostAndroid
    : public DocumentUserData<TextSuggestionHostAndroid> {
 public:
  ~TextSuggestionHostAndroid() override;

  // Called from the Java text suggestion menu to have Blink apply a spell
  // check suggestion.
  void ApplySpellCheckSuggestion(
      JNIEnv*,
      const base::android::JavaRef<jstring>& replacement);
  // Called from the Java text suggestion menu to have Blink apply a text
  // suggestion.
  void ApplyTextSuggestion(JNIEnv*,
                           int marker_tag,
                           int suggestion_index);
  // Called from the Java text suggestion menu to have Blink delete the
  // currently highlighted region of text that the open suggestion menu pertains
  // to.
  void DeleteActiveSuggestionRange(JNIEnv*);
  // Called from the Java text suggestion menu to tell Blink that a word is
  // being added to the dictionary (so Blink can clear the spell check markers
  // for that word).
  void OnNewWordAddedToDictionary(JNIEnv*,
                                  const base::android::JavaRef<jstring>& word);
  // Called from the Java text suggestion menu to tell Blink that the user
  // closed the menu without performing one of the available actions, so Blink
  // can re-show the insertion caret and remove the suggestion range highlight.
  void OnSuggestionMenuClosed(JNIEnv*);
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

  // Called to trigger any open popups to close.
  void HidePopups();
  // Called by Blink when the user taps on a spell check marker and we might
  // want to show the text suggestion menu after the double-tap timer expires.
  void StartSuggestionMenuTimer();
  // Called by browser-side code in response to an input event to stop the
  // suggestion menu timer.
  void StopSuggestionMenuTimer();

  void BindTextSuggestionHost(
      mojo::PendingReceiver<blink::mojom::TextSuggestionHost> receiver);

 private:
  friend class DocumentUserData<TextSuggestionHostAndroid>;
  DOCUMENT_USER_DATA_KEY_DECL();

  explicit TextSuggestionHostAndroid(RenderFrameHost* rfh);

  const mojo::Remote<blink::mojom::TextSuggestionBackend>&
  GetTextSuggestionBackend();
  // Used by the spell check menu timer to notify Blink that the timer has
  // expired.
  void OnSuggestionMenuTimeout();

  mojo::Remote<blink::mojom::TextSuggestionBackend> text_suggestion_backend_;
  std::unique_ptr<TextSuggestionHostMojoImplAndroid> text_suggestion_impl_;
  input::TimeoutMonitor suggestion_menu_timeout_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_TEXT_SUGGESTION_HOST_ANDROID_H_
