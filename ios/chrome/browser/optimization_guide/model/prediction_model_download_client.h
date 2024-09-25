// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_PREDICTION_MODEL_DOWNLOAD_CLIENT_H_
#define IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_PREDICTION_MODEL_DOWNLOAD_CLIENT_H_

#import "base/memory/raw_ptr.h"
#import "components/download/public/background_service/client.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace download {
struct CompletionInfo;
struct DownloadMetaData;
}  // namespace download

namespace optimization_guide {

class PredictionModelDownloadManager;

class PredictionModelDownloadClient : public download::Client {
 public:
  explicit PredictionModelDownloadClient(ProfileIOS* profile);
  ~PredictionModelDownloadClient() override;
  PredictionModelDownloadClient(const PredictionModelDownloadClient&) = delete;
  PredictionModelDownloadClient& operator=(
      const PredictionModelDownloadClient&) = delete;

  // download::Client:
  void OnServiceInitialized(
      bool state_lost,
      const std::vector<download::DownloadMetaData>& downloads) override;
  void OnServiceUnavailable() override;
  void OnDownloadStarted(
      const std::string& guid,
      const std::vector<GURL>& url_chain,
      const scoped_refptr<const net::HttpResponseHeaders>& headers) override;
  void OnDownloadFailed(const std::string& guid,
                        const download::CompletionInfo& completion_info,
                        download::Client::FailureReason reason) override;
  void OnDownloadSucceeded(
      const std::string& guid,
      const download::CompletionInfo& completion_info) override;
  bool CanServiceRemoveDownloadedFile(const std::string& guid,
                                      bool force_delete) override;
  void GetUploadData(const std::string& guid,
                     download::GetUploadDataCallback callback) override;

 private:
  // Returns the PredictionModelDownloadManager for the profile.
  PredictionModelDownloadManager* GetPredictionModelDownloadManager();

  raw_ptr<ProfileIOS> profile_;
};

}  // namespace optimization_guide

#endif  // IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_PREDICTION_MODEL_DOWNLOAD_CLIENT_H_
