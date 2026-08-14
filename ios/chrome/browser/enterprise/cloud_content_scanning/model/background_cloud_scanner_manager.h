// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_BACKGROUND_CLOUD_SCANNER_MANAGER_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_BACKGROUND_CLOUD_SCANNER_MANAGER_H_

#import <memory>
#import <vector>

#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/sequence_checker.h"
#import "components/enterprise/connectors/core/cloud_content_scanning/common.h"
#import "components/keyed_service/core/keyed_service.h"

class GURL;
class ProfileIOS;

namespace base {
class FilePath;
}  // namespace base

namespace enterprise_connectors {

class BinaryUploadService;
class ContentAnalysisInfo;
class FilesRequestHandlerBase;

// Helper manager attached to ProfileIOS (as a KeyedService) to own active
// background scans. Tying active background scans to the profile's lifetime
// (instead of the tab helper) ensures that:
// 1. Scans survive tab closures to maintain enterprise compliance and auditing.
// 2. All pending scans are safely cancelled and destroyed when the profile is
//    destroyed, preventing use-after-free/dangling raw_ptr<ProfileIOS> issues.
//
// // TODO(crbug.com/545744370): Remove the ProfileIOS dependency and inject the
// service directly. To remove the ProfileIOS dependency, we'll need to
// refactor `FilesRequestHandlerIOS` first.
class BackgroundCloudScannerManager : public KeyedService {
 public:
  BackgroundCloudScannerManager(ProfileIOS* profile,
                                BinaryUploadService* upload_service);

  BackgroundCloudScannerManager(const BackgroundCloudScannerManager&) = delete;
  BackgroundCloudScannerManager& operator=(
      const BackgroundCloudScannerManager&) = delete;

  ~BackgroundCloudScannerManager() override;

  // This is used for non-blocking scans where the UI proceeds immediately but
  // the scan must still run to completion and report audit verdicts.
  void StartScanner(std::unique_ptr<ContentAnalysisInfo> info,
                    const GURL& url,
                    const base::FilePath& file_path);

  base::WeakPtr<BackgroundCloudScannerManager> GetWeakPtr();

 private:
  // Nested helper class representing a single active background scan request.
  // It is placed in the private section to encapsulate all scanning execution
  // details away from external consumers.
  class BackgroundCloudScanner {
   public:
    using OnCompleteCallback =
        base::OnceCallback<void(BackgroundCloudScanner*)>;

    // Constructor, destructor, and Start() are public solely to allow standard
    // container templates (like std::unique_ptr and std::vector) to manage
    // their lifecycles, as the nested class access bypass does not apply to std
    // library templates.
    BackgroundCloudScanner(std::unique_ptr<ContentAnalysisInfo> info,
                           ProfileIOS* profile,
                           BinaryUploadService* upload_service,
                           const GURL& url,
                           const base::FilePath& file_path,
                           OnCompleteCallback on_complete_callback);

    ~BackgroundCloudScanner();

    BackgroundCloudScanner(const BackgroundCloudScanner&) = delete;
    BackgroundCloudScanner& operator=(const BackgroundCloudScanner&) = delete;

    // Triggers the asynchronous upload scan request. Separated from the
    // constructor to guarantee safe, fully-constructed, and owned registration
    // before any side-effects can run.
    void Start();

   private:
    // Runs when the scan completes. Invokes the completion callback
    // synchronously.
    void OnScanComplete(RequestHandlerResult result);

    std::unique_ptr<ContentAnalysisInfo> info_;
    std::unique_ptr<FilesRequestHandlerBase> handler_;
    OnCompleteCallback on_complete_callback_;
    base::WeakPtrFactory<BackgroundCloudScanner> weak_ptr_factory_{this};
  };

  // Registers an active background scanner to be owned by this manager.
  void AddScanner(std::unique_ptr<BackgroundCloudScanner> scanner);

  // Unregisters and schedules the safe, asynchronous destruction of a scanner
  // to avoid use-after-free/re-entrancy during active callback execution.
  void RemoveScanner(BackgroundCloudScanner* scanner);

  raw_ptr<ProfileIOS> profile_;
  raw_ptr<BinaryUploadService> upload_service_;

  std::vector<std::unique_ptr<BackgroundCloudScanner>> scanners_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<BackgroundCloudScannerManager> weak_ptr_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_BACKGROUND_CLOUD_SCANNER_MANAGER_H_
