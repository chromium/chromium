// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_MODEL_AI_PROTOTYPING_SERVICE_IMPL_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_MODEL_AI_PROTOTYPING_SERVICE_IMPL_H_

#import "components/optimization_guide/optimization_guide_buildflags.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/optimization_guide/mojom/ai_prototyping_service.mojom.h"
#import "ios/web/public/web_state.h"
#import "mojo/public/cpp/bindings/receiver.h"

namespace ai {

class AIPrototypingServiceImpl : public mojom::AIPrototypingService {
 public:
  explicit AIPrototypingServiceImpl(
      mojo::PendingReceiver<mojom::AIPrototypingService> receiver,
      web::BrowserState* browser_state,
      bool start_on_device);
  ~AIPrototypingServiceImpl() override;
  AIPrototypingServiceImpl(const AIPrototypingServiceImpl&) = delete;
  AIPrototypingServiceImpl& operator=(const AIPrototypingServiceImpl&) = delete;

  // ai::mojom::AIPrototypingServiceImpl:
  void ExecuteServerQuery(::mojo_base::ProtoWrapper request,
                          ExecuteServerQueryCallback callback) override;
  void ExecuteOnDeviceQuery(::mojo_base::ProtoWrapper request,
                            ExecuteOnDeviceQueryCallback callback) override;

 private:
#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)

  // Handles the response from an on-device query execution.
  std::string OnDeviceModelExecuteResponse(
      optimization_guide::OptimizationGuideModelStreamingExecutionResult
          result);

  // Attempts to create an on-device session. If the feature's configuration
  // hasn't been downloaded yet, this will trigger that download and fail to
  // start the session. Once the configuration download is complete, the session
  // will be able to be started successfully.
  void StartOnDeviceSession();

  // Retains the on-device session in memory.
  std::unique_ptr<optimization_guide::OnDeviceSession> on_device_session_;
#endif  // BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)

  // Model execution completed callback.
  void OnExecuteModelWithLoggingCallback(
      ExecuteServerQueryCallback query_callback,
      optimization_guide::OptimizationGuideModelExecutionResult
          execution_result,
      std::unique_ptr<optimization_guide::proto::BlingPrototypingLoggingData>
          logging_data);

  // Processes the response from a server-hosted query execution.
  std::string ProcessServerModelExecuteResponse(
      optimization_guide::OptimizationGuideModelExecutionResult result);

  // Service used to execute LLM queries.
  raw_ptr<OptimizationGuideService> service_;
  mojo::Receiver<mojom::AIPrototypingService> receiver_;

  base::WeakPtrFactory<AIPrototypingServiceImpl> weak_ptr_factory_{this};
};

}  // namespace ai

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_MODEL_AI_PROTOTYPING_SERVICE_IMPL_H_
