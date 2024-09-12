// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_FAKE_DOWNLOAD_ITEM_H_
#define CONTENT_PUBLIC_TEST_FAKE_DOWNLOAD_ITEM_H_

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_source.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class FakeDownloadItem : public download::DownloadItem {
 public:
  FakeDownloadItem();

  FakeDownloadItem(const FakeDownloadItem&) = delete;
  FakeDownloadItem& operator=(const FakeDownloadItem&) = delete;

  ~FakeDownloadItem() override;

  // download::DownloadItem overrides.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void UpdateObservers() override;
  void Remove() override;
  void Pause() override;
  void Resume(bool user_resume) override;
  void Cancel(bool user_cancel) override;
  void OpenDownload() override;
  void ShowDownloadInShell() override;
  uint32_t GetId() const override;
  const std::string& GetGuid() const override;
  const GURL& GetURL() const override;
  const std::vector<GURL>& GetUrlChain() const override;
  const base::FilePath& GetTargetFilePath() const override;
  bool GetFileExternallyRemoved() const override;
  base::Time GetStartTime() const override;
  base::Time GetEndTime() const override;
  DownloadState GetState() const override;
  const scoped_refptr<const net::HttpResponseHeaders>& GetResponseHeaders()
      const override;
  std::string GetMimeType() const override;
  const GURL& GetOriginalUrl() const override;
  download::DownloadInterruptReason GetLastReason() const override;
  int64_t GetReceivedBytes() const override;
  int64_t GetTotalBytes() const override;
  base::Time GetLastAccessTime() const override;
  bool IsTransient() const override;
  bool IsParallelDownload() const override;
  DownloadCreationType GetDownloadCreationType() const override;
  ::network::mojom::CredentialsMode GetCredentialsMode() const override;
  const std::optional<net::IsolationInfo>& GetIsolationInfo() const override;
  bool IsDone() const override;
  const std::string& GetETag() const override;
  const std::string& GetLastModifiedTime() const override;
  bool IsPaused() const override;
  bool AllowMetered() const override;
  bool IsTemporary() const override;
  bool RequireSafetyChecks() const override;
  bool CanResume() const override;
  int64_t GetBytesWasted() const override;
  int32_t GetAutoResumeCount() const override;
  const GURL& GetReferrerUrl() const override;
  const std::string& GetSerializedEmbedderDownloadData() const override;
  const GURL& GetTabUrl() const override;
  const GURL& GetTabReferrerUrl() const override;
  const std::optional<url::Origin>& GetRequestInitiator() const override;
  std::string GetSuggestedFilename() const override;
  std::string GetContentDisposition() const override;
  std::string GetOriginalMimeType() const override;
  std::string GetRemoteAddress() const override;
  bool HasUserGesture() const override;
  ui::PageTransition GetTransitionType() const override;
  bool IsSavePackageDownload() const override;
  download::DownloadSource GetDownloadSource() const override;
  const base::FilePath& GetFullPath() const override;
  const base::FilePath& GetForcedFilePath() const override;
  base::FilePath GetTemporaryFilePath() const override;
  base::FilePath GetFileNameToReportUser() const override;
  TargetDisposition GetTargetDisposition() const override;
  const std::string& GetHash() const override;
  void DeleteFile(base::OnceCallback<void(bool)> callback) override;
  download::DownloadFile* GetDownloadFile() override;
  download::DownloadItemRenameHandler* GetRenameHandler() override;
#if BUILDFLAG(IS_ANDROID)
  bool IsFromExternalApp() override;
  bool IsMustDownload() override;
#endif  // BUILDFLAG(IS_ANDROID)
  bool IsDangerous() const override;
  bool IsInsecure() const override;
  download::DownloadDangerType GetDangerType() const override;
  download::DownloadItem::InsecureDownloadStatus GetInsecureDownloadStatus()
      const override;
  bool TimeRemaining(base::TimeDelta* remaining) const override;
  int64_t CurrentSpeed() const override;
  int PercentComplete() const override;
  bool AllDataSaved() const override;
  const std::vector<download::DownloadItem::ReceivedSlice>& GetReceivedSlices()
      const override;
  int64_t GetUploadedBytes() const override;
  bool CanShowInFolder() override;
  bool CanOpenDownload() override;
  bool ShouldOpenFileBasedOnExtension() override;
  bool ShouldOpenFileByPolicyBasedOnExtension() override;
  bool GetOpenWhenComplete() const override;
  bool GetAutoOpened() override;
  bool GetOpened() const override;
  void OnContentCheckCompleted(
      download::DownloadDangerType danger_type,
      download::DownloadInterruptReason reason) override;
  void SetOpenWhenComplete(bool open) override;
  void SetOpened(bool opened) override;
  void SetLastAccessTime(base::Time time) override;
  void SetDisplayName(const base::FilePath& name) override;
  std::string DebugString(bool verbose) const override;
  void SimulateErrorForTesting(
      download::DownloadInterruptReason reason) override;
  void ValidateDangerousDownload() override;
  void ValidateInsecureDownload() override;
  void CopyDownload(AcquireFileCallback callback) override;
  void Rename(const base::FilePath& name,
              RenameDownloadCallback callback) override;
  void OnAsyncScanningCompleted(
      download::DownloadDangerType danger_type) override;

  bool removed() const { return removed_; }

  void NotifyDownloadDestroyed();
  void NotifyDownloadRemoved();
  void NotifyDownloadUpdated();

  void SetId(uint32_t id);
  void SetGuid(const std::string& guid);
  void SetURL(const GURL& url);
  void SetUrlChain(const std::vector<GURL>& url_chain);
  void SetTargetFilePath(const base::FilePath& file_path);
  void SetFileExternallyRemoved(bool is_file_externally_removed);
  void SetStartTime(base::Time start_time);
  void SetEndTime(base::Time end_time);
  void SetState(const DownloadState& state);
  void SetResponseHeaders(
      scoped_refptr<const net::HttpResponseHeaders> response_headers);
  void SetMimeType(const std::string& mime_type);
  void SetOriginalUrl(const GURL& url);
  void SetTabUrl(const GURL& url);
  void SetLastReason(download::DownloadInterruptReason last_reason);
  void SetReceivedBytes(int64_t received_bytes);
  void SetTotalBytes(int64_t total_bytes);
  void SetIsTransient(bool is_transient);
  void SetIsParallelDownload(bool is_parallel_download);
  void SetIsDone(bool is_done);
  void SetETag(const std::string& etag);
  void SetLastModifiedTime(const std::string& last_modified_time);
  void SetHash(const std::string& hash);
  void SetPercentComplete(int percent_complete);
  void SetDummyFilePath(const base::FilePath& dummy_file_path);
  void SetIsDangerous(bool is_dangerous);
  void SetIsInsecure(bool is_insecure);
  void SetDangerType(download::DownloadDangerType danger_type);
  void SetInsecureDownloadStatus(
      download::DownloadItem::InsecureDownloadStatus insecure_download_status);

 private:
  base::ObserverList<Observer> observers_;
  uint32_t id_ = 0;
  std::string guid_;
  GURL url_;
  std::vector<GURL> url_chain_;
  base::FilePath file_path_;
  bool is_file_externally_removed_ = false;
  bool removed_ = false;
  base::Time start_time_;
  base::Time end_time_;
  base::Time last_access_time_;
  // MAX_DOWNLOAD_STATE is used as the uninitialized state.
  DownloadState download_state_ =
      download::DownloadItem::DownloadState::MAX_DOWNLOAD_STATE;
  scoped_refptr<const net::HttpResponseHeaders> response_headers_;
  std::string mime_type_;
  GURL original_url_;
  GURL tab_url_;
  download::DownloadInterruptReason last_reason_ =
      download::DOWNLOAD_INTERRUPT_REASON_NONE;
  int64_t received_bytes_ = 0;
  int64_t uploaded_bytes_ = 0;
  int64_t total_bytes_ = 0;
  bool is_transient_ = false;
  bool is_parallel_download_ = false;
  bool is_done_ = false;
  std::string etag_;
  std::string last_modified_time_;
  std::string hash_;
  int percent_complete_ = 0;
  bool open_when_complete_ = false;
  bool is_dangerous_ = false;
  bool is_insecure_ = false;
  std::optional<net::IsolationInfo> isolation_info_;
  download::DownloadDangerType danger_type_ =
      download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS;
  download::DownloadItem::InsecureDownloadStatus insecure_download_status_ =
      download::DownloadItem::InsecureDownloadStatus::UNKNOWN;

  // The members below are to be returned by methods, which return by reference.
  GURL dummy_url;
  std::string serialized_embedder_download_data;
  std::optional<url::Origin> dummy_origin;
  base::FilePath dummy_file_path;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_FAKE_DOWNLOAD_ITEM_H_
