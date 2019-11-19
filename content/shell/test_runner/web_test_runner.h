// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_TEST_RUNNER_WEB_TEST_RUNNER_H_
#define CONTENT_SHELL_TEST_RUNNER_WEB_TEST_RUNNER_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"

class SkBitmap;

namespace base {
class DictionaryValue;
}

namespace blink {
class WebContentSettingsClient;
class WebLocalFrame;
class WebTextCheckClient;
class WebView;
}

namespace content {
class RenderView;
}

namespace test_runner {

class WebTestRunner {
 public:
  // Returns a mock WebContentSettings that is used for web tests. An
  // embedder should use this for all WebViews it creates.
  virtual blink::WebContentSettingsClient* GetWebContentSettings() const = 0;

  // Returns a mock WebTextCheckClient that is used for web tests. An
  // embedder should use this for all WebLocalFrames it creates.
  virtual blink::WebTextCheckClient* GetWebTextCheckClient() const = 0;

  // After WebTestDelegate::TestFinished was invoked, the following methods
  // can be used to determine what kind of dump the main WebViewTestProxy can
  // provide.

  // If true, WebTestDelegate::audioData returns an audio dump and no text
  // or pixel results are available.
  virtual bool ShouldDumpAsAudio() const = 0;
  virtual void GetAudioData(std::vector<unsigned char>* buffer_view) const = 0;

  // Reports if tests requested a recursive layout dump of all frames
  // (i.e. by calling testRunner.dumpChildFramesAsText() from javascript).
  virtual bool IsRecursiveLayoutDumpRequested() = 0;

  // Dumps layout of |frame| using the mode requested by the current test
  // (i.e. text mode if testRunner.dumpAsText() was called from javascript).
  virtual std::string DumpLayout(blink::WebLocalFrame* frame) = 0;

  // Returns true if the selection window should be painted onto captured
  // pixels.
  virtual bool ShouldDumpSelectionRect() const = 0;

  // Returns false if the browser should capture the pixel output, true if it
  // can be done locally in the renderer via DumpPixelsAsync().
  virtual bool CanDumpPixelsFromRenderer() const = 0;

  // Snapshots the content of |render_view| using the mode requested by the
  // current test and calls |callback| with the result.  Caller needs to ensure
  // that |render_view| stays alive until |callback| is called.
  virtual void DumpPixelsAsync(
      content::RenderView* render_view,
      base::OnceCallback<void(const SkBitmap&)> callback) = 0;

  // Replicates changes to web test runtime flags
  // (i.e. changes that happened in another renderer).
  // See also WebTestDelegate::OnWebTestRuntimeFlagsChanged.
  virtual void ReplicateWebTestRuntimeFlagsChanges(
      const base::DictionaryValue& changed_values) = 0;

  // If custom text dump is present (i.e. if testRunner.setCustomTextOutput has
  // been called from javascript), then returns |true| and populates the
  // |custom_text_dump| argument.  Otherwise returns |false|.
  virtual bool HasCustomTextDump(std::string* custom_text_dump) const = 0;

  // Returns true if the call to WebViewTestProxy::captureTree will invoke
  // WebTestDelegate::captureHistoryForWindow.
  virtual bool ShouldDumpBackForwardList() const = 0;

  // Returns true if WebViewTestProxy::capturePixels should be invoked after
  // capturing text results.
  virtual bool ShouldGeneratePixelResults() = 0;

  // Sets focus on the given view.  Internally tracks currently focused view,
  // to aid in defocusing previously focused views at the right time.
  virtual void SetFocus(blink::WebView* web_view, bool focus) = 0;
};

}  // namespace test_runner

#endif  // CONTENT_SHELL_TEST_RUNNER_WEB_TEST_RUNNER_H_
