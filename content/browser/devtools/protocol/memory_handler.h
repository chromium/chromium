// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_MEMORY_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_MEMORY_HANDLER_H_

#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/memory.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/leak_detector/leak_detector.mojom.h"

namespace content {
namespace protocol {

class MemoryHandler : public DevToolsDomainHandler,
                      public Memory::Backend {
 public:
  MemoryHandler();

  MemoryHandler(const MemoryHandler&) = delete;
  MemoryHandler& operator=(const MemoryHandler&) = delete;

  ~MemoryHandler() override;

  void Wire(UberDispatcher* dispatcher) override;
  void SetRenderer(int process_host_id,
                   RenderFrameHostImpl* frame_host) override;

  Response GetBrowserSamplingProfile(
      std::unique_ptr<Memory::SamplingProfile>* out_profile) override;
  Response SetPressureNotificationsSuppressed(bool suppressed) override;
  Response SimulatePressureNotification(const std::string& level) override;
  void PrepareForLeakDetection(
      std::unique_ptr<PrepareForLeakDetectionCallback> callback) override;
  void GetDOMCountersForLeakDetection(
      std::unique_ptr<GetDOMCountersForLeakDetectionCallback> callback)
      override;

 private:
  void RequestLeakDetection(RenderProcessHost* process);
  void OnLeakDetectionComplete(blink::mojom::LeakDetectionResultPtr result);
  void OnLeakDetectorIsGone();
  std::unique_ptr<protocol::Array<protocol::Memory::DOMCounter>> GetDOMCounters(
      const blink::mojom::LeakDetectionResult& result);

  int process_host_id_;
  mojo::Remote<blink::mojom::LeakDetector> leak_detector_;
  std::unique_ptr<PrepareForLeakDetectionCallback>
      prepare_for_leak_detection_callback_;
  std::unique_ptr<GetDOMCountersForLeakDetectionCallback>
      get_dom_counters_for_leak_detection_callback_;
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_MEMORY_HANDLER_H_
