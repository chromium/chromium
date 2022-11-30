// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_BROWSER_LEAK_DETECTOR_H_
#define CONTENT_WEB_TEST_BROWSER_LEAK_DETECTOR_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/leak_detector/leak_detector.mojom.h"

namespace content {

class RenderProcessHost;

// LeakDetector counts DOM objects and compare them between two pages.
class LeakDetector {
 public:
  LeakDetector();

  LeakDetector(const LeakDetector&) = delete;
  LeakDetector& operator=(const LeakDetector&) = delete;

  ~LeakDetector();

  struct LeakDetectionReport {
    bool leaked;
    std::string detail;
  };
  using ReportCallback = base::OnceCallback<void(const LeakDetectionReport&)>;

  // Counts DOM objects, compare the previous status and returns the result of
  // leak detection. It is assumed that this method is always called when a
  // specific page, like about:blank is loaded to compare the previous
  // circumstance of DOM objects. If the number of objects increases, there
  // should be a leak.
  void TryLeakDetection(RenderProcessHost* host, ReportCallback callback);

 private:
  void OnLeakDetectionComplete(blink::mojom::LeakDetectionResultPtr result);
  void OnLeakDetectorIsGone();

  mojo::Remote<blink::mojom::LeakDetector> leak_detector_;
  blink::mojom::LeakDetectionResultPtr previous_result_;
  ReportCallback callback_;
  base::WeakPtrFactory<LeakDetector> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_BROWSER_LEAK_DETECTOR_H_
