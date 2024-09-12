// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/fake_download_item.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "components/download/public/common/download_danger_type.h"
#include "net/http/http_response_headers.h"

namespace content {

FakeDownloadItem::FakeDownloadItem() = default;

FakeDownloadItem::~FakeDownloadItem() {
  NotifyDownloadRemoved();
  NotifyDownloadDestroyed();
}

void FakeDownloadItem::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeDownloadItem::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FakeDownloadItem::NotifyDownloadDestroyed() {
  for (auto& observer : observers_) {
    observer.OnDownloadDestroyed(this);
  }
}

void FakeDownloadItem::NotifyDownloadRemoved() {
  for (auto& observer : observers_) {
    observer.OnDownloadRemoved(this);
  }
}

void FakeDownloadItem::NotifyDownloadUpdated() {
  UpdateObservers();
}

void FakeDownloadItem::UpdateObservers() {
  for (auto& observer : observers_) {
    observer.OnDownloadUpdated(this);
  }
}

void FakeDownloadItem::SetId(uint32_t id) {
  id_ = id;
}

uint32_t FakeDownloadItem::GetId() const {
  return id_;
}

void FakeDownloadItem::SetGuid(const std::string& guid) {
  guid_ = guid;
}

const std::string& FakeDownloadItem::GetGuid() const {
  return guid_;
}

void FakeDownloadItem::SetURL(const GURL& url) {
  url_ = url;
}

const GURL& FakeDownloadItem::GetURL() const {
  return url_;
}

void FakeDownloadItem::SetUrlChain(const std::vector<GURL>& url_chain) {
  url_chain_ = url_chain;
}

const std::vector<GURL>& FakeDownloadItem::GetUrlChain() const {
  return url_chain_;
}

void FakeDownloadItem::SetLastReason(
    download::DownloadInterruptReason last_reason) {
  last_reason_ = last_reason;
}

download::DownloadInterruptReason FakeDownloadItem::GetLastReason() const {
  return last_reason_;
}

void FakeDownloadItem::SetTargetFilePath(const base::FilePath& file_path) {
  file_path_ = file_path;
}

const base::FilePath& FakeDownloadItem::GetTargetFilePath() const {
  return file_path_;
}

void FakeDownloadItem::SetFileExternallyRemoved(
    bool is_file_externally_removed) {
  is_file_externally_removed_ = is_file_externally_removed;
}

bool FakeDownloadItem::GetFileExternallyRemoved() const {
  return is_file_externally_removed_;
}

void FakeDownloadItem::SetStartTime(base::Time start_time) {
  start_time_ = start_time;
}

base::Time FakeDownloadItem::GetStartTime() const {
  return start_time_;
}

void FakeDownloadItem::SetEndTime(base::Time end_time) {
  end_time_ = end_time;
}

base::Time FakeDownloadItem::GetEndTime() const {
  return end_time_;
}

void FakeDownloadItem::SetState(const DownloadState& state) {
  download_state_ = state;
}

download::DownloadItem::DownloadState FakeDownloadItem::GetState() const {
  return download_state_;
}

void FakeDownloadItem::SetResponseHeaders(
    scoped_refptr<const net::HttpResponseHeaders> response_headers) {
  response_headers_ = response_headers;
}

const scoped_refptr<const net::HttpResponseHeaders>&
FakeDownloadItem::GetResponseHeaders() const {
  return response_headers_;
}

void FakeDownloadItem::SetMimeType(const std::string& mime_type) {
  mime_type_ = mime_type;
}

std::string FakeDownloadItem::GetMimeType() const {
  return mime_type_;
}

void FakeDownloadItem::SetOriginalUrl(const GURL& url) {
  original_url_ = url;
}

void FakeDownloadItem::SetTabUrl(const GURL& url) {
  tab_url_ = url;
}

const GURL& FakeDownloadItem::GetOriginalUrl() const {
  return original_url_;
}

void FakeDownloadItem::SetReceivedBytes(int64_t received_bytes) {
  received_bytes_ = received_bytes;
}

void FakeDownloadItem::SetTotalBytes(int64_t total_bytes) {
  total_bytes_ = total_bytes;
}

int64_t FakeDownloadItem::GetReceivedBytes() const {
  return received_bytes_;
}

void FakeDownloadItem::SetLastAccessTime(base::Time time) {
  last_access_time_ = time;
}

base::Time FakeDownloadItem::GetLastAccessTime() const {
  return last_access_time_;
}

void FakeDownloadItem::SetIsTransient(bool is_transient) {
  is_transient_ = is_transient;
}

bool FakeDownloadItem::IsTransient() const {
  return is_transient_;
}

void FakeDownloadItem::SetIsParallelDownload(bool is_parallel_download) {
  is_parallel_download_ = is_parallel_download;
}

bool FakeDownloadItem::IsParallelDownload() const {
  return is_parallel_download_;
}

download::DownloadItem::DownloadCreationType
FakeDownloadItem::GetDownloadCreationType() const {
  return download::DownloadItem::DownloadCreationType::TYPE_ACTIVE_DOWNLOAD;
}

::network::mojom::CredentialsMode FakeDownloadItem::GetCredentialsMode() const {
  return ::network::mojom::CredentialsMode::kInclude;
}

const std::optional<net::IsolationInfo>& FakeDownloadItem::GetIsolationInfo()
    const {
  return isolation_info_;
}

void FakeDownloadItem::SetIsDone(bool is_done) {
  is_done_ = is_done;
}

bool FakeDownloadItem::IsDone() const {
  return is_done_;
}

void FakeDownloadItem::SetETag(const std::string& etag) {
  etag_ = etag;
}

const std::string& FakeDownloadItem::GetETag() const {
  return etag_;
}

void FakeDownloadItem::SetLastModifiedTime(
    const std::string& last_modified_time) {
  last_modified_time_ = last_modified_time;
}

void FakeDownloadItem::SetHash(const std::string& hash) {
  hash_ = hash;
}

const std::string& FakeDownloadItem::GetLastModifiedTime() const {
  return last_modified_time_;
}

void FakeDownloadItem::SetPercentComplete(int percent_complete) {
  percent_complete_ = percent_complete;
}

int FakeDownloadItem::PercentComplete() const {
  return percent_complete_;
}

void FakeDownloadItem::SetDummyFilePath(const base::FilePath& file_path) {
  dummy_file_path = file_path;
}

void FakeDownloadItem::SetIsDangerous(bool is_dangerous) {
  is_dangerous_ = is_dangerous;
}

void FakeDownloadItem::SetIsInsecure(bool is_insecure) {
  is_insecure_ = is_insecure;
}

void FakeDownloadItem::SetDangerType(download::DownloadDangerType danger_type) {
  danger_type_ = danger_type;
}

void FakeDownloadItem::SetInsecureDownloadStatus(
    download::DownloadItem::InsecureDownloadStatus insecure_download_status) {
  insecure_download_status_ = insecure_download_status;
}

bool FakeDownloadItem::GetOpenWhenComplete() const {
  return open_when_complete_;
}

void FakeDownloadItem::SetOpenWhenComplete(bool open) {
  open_when_complete_ = open;
}

// The methods below are not supported and are not expected to be called.
void FakeDownloadItem::ValidateDangerousDownload() {
  NOTREACHED_IN_MIGRATION();
}

void FakeDownloadItem::ValidateInsecureDownload() {
  NOTREACHED_IN_MIGRATION();
}

void FakeDownloadItem::CopyDownload(AcquireFileCallback callback) {
  NOTREACHED_IN_MIGRATION();
  std::move(callback).Run(base::FilePath());
}

void FakeDownloadItem::Pause() {
  NOTREACHED_IN_MIGRATION();
}

void FakeDownloadItem::Resume(bool user_resume) {
  NOTREACHED_IN_MIGRATION();
}

void FakeDownloadItem::Cancel(bool user_cancel) {
  NOTREACHED_IN_MIGRATION();
}

void FakeDownloadItem::Remove() {
  removed_ = true;
}

void FakeDownloadItem::OpenDownload() {
  NOTREACHED_IN_MIGRATION();
}

void FakeDownloadItem::ShowDownloadInShell() {
  NOTREACHED_IN_MIGRATION();
}

void FakeDownloadItem::Rename(const base::FilePath& name,
                              RenameDownloadCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void FakeDownloadItem::OnAsyncScanningCompleted(
    download::DownloadDangerType danger_type) {
  NOTREACHED_IN_MIGRATION();
}

bool FakeDownloadItem::IsPaused() const {
  return false;
}

bool FakeDownloadItem::AllowMetered() const {
  NOTREACHED_IN_MIGRATION();
  return false;
}

bool FakeDownloadItem::IsTemporary() const {
  NOTREACHED_IN_MIGRATION();
  return false;
}

bool FakeDownloadItem::RequireSafetyChecks() const {
  NOTREACHED_IN_MIGRATION();
  return false;
}

bool FakeDownloadItem::CanResume() const {
  NOTREACHED_IN_MIGRATION();
  return false;
}

int64_t FakeDownloadItem::GetBytesWasted() const {
  NOTREACHED_IN_MIGRATION();
  return 0;
}

int32_t FakeDownloadItem::GetAutoResumeCount() const {
  NOTREACHED_IN_MIGRATION();
  return 0;
}

const GURL& FakeDownloadItem::GetReferrerUrl() const {
  NOTREACHED_IN_MIGRATION();
  return dummy_url;
}

const std::string& FakeDownloadItem::GetSerializedEmbedderDownloadData() const {
  NOTREACHED_IN_MIGRATION();
  return serialized_embedder_download_data;
}

const GURL& FakeDownloadItem::GetTabUrl() const {
  return tab_url_;
}

const GURL& FakeDownloadItem::GetTabReferrerUrl() const {
  NOTREACHED_IN_MIGRATION();
  return dummy_url;
}

const std::optional<url::Origin>& FakeDownloadItem::GetRequestInitiator()
    const {
  NOTREACHED_IN_MIGRATION();
  return dummy_origin;
}

std::string FakeDownloadItem::GetSuggestedFilename() const {
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

std::string FakeDownloadItem::GetContentDisposition() const {
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

std::string FakeDownloadItem::GetOriginalMimeType() const {
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

std::string FakeDownloadItem::GetRemoteAddress() const {
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

bool FakeDownloadItem::HasUserGesture() const {
  NOTREACHED_IN_MIGRATION();
  return false;
}

ui::PageTransition FakeDownloadItem::GetTransitionType() const {
  NOTREACHED_IN_MIGRATION();
  return ui::PageTransition();
}

bool FakeDownloadItem::IsSavePackageDownload() const {
  NOTREACHED_IN_MIGRATION();
  return false;
}

download::DownloadSource FakeDownloadItem::GetDownloadSource() const {
  return download::DownloadSource::UNKNOWN;
}

const base::FilePath& FakeDownloadItem::GetFullPath() const {
  return dummy_file_path;
}

const base::FilePath& FakeDownloadItem::GetForcedFilePath() const {
  NOTREACHED_IN_MIGRATION();
  return dummy_file_path;
}

base::FilePath FakeDownloadItem::GetTemporaryFilePath() const {
  NOTREACHED_IN_MIGRATION();
  return dummy_file_path;
}

base::FilePath FakeDownloadItem::GetFileNameToReportUser() const {
  NOTREACHED_IN_MIGRATION();
  return base::FilePath();
}

download::DownloadItem::TargetDisposition
FakeDownloadItem::GetTargetDisposition() const {
  NOTREACHED_IN_MIGRATION();
  return TargetDisposition();
}

const std::string& FakeDownloadItem::GetHash() const {
  return hash_;
}

void FakeDownloadItem::DeleteFile(base::OnceCallback<void(bool)> callback) {
  NOTREACHED_IN_MIGRATION();
}

download::DownloadFile* FakeDownloadItem::GetDownloadFile() {
  return nullptr;
}

download::DownloadItemRenameHandler* FakeDownloadItem::GetRenameHandler() {
  return nullptr;
}

#if BUILDFLAG(IS_ANDROID)
bool FakeDownloadItem::IsFromExternalApp() {
  return false;
}

bool FakeDownloadItem::IsMustDownload() {
  return false;
}
#endif  // BUILDFLAG(IS_ANDROID)

bool FakeDownloadItem::IsDangerous() const {
  return is_dangerous_;
}

bool FakeDownloadItem::IsInsecure() const {
  return is_insecure_;
}

download::DownloadDangerType FakeDownloadItem::GetDangerType() const {
  return danger_type_;
}

download::DownloadItem::InsecureDownloadStatus
FakeDownloadItem::GetInsecureDownloadStatus() const {
  return insecure_download_status_;
}

bool FakeDownloadItem::TimeRemaining(base::TimeDelta* remaining) const {
  NOTREACHED_IN_MIGRATION();
  return false;
}

int64_t FakeDownloadItem::CurrentSpeed() const {
  NOTREACHED_IN_MIGRATION();
  return 1;
}

bool FakeDownloadItem::AllDataSaved() const {
  NOTREACHED_IN_MIGRATION();
  return true;
}

int64_t FakeDownloadItem::GetTotalBytes() const {
  return total_bytes_;
}

const std::vector<download::DownloadItem::ReceivedSlice>&
FakeDownloadItem::GetReceivedSlices() const {
  NOTREACHED_IN_MIGRATION();
  static const std::vector<download::DownloadItem::ReceivedSlice> slices;
  return slices;
}

int64_t FakeDownloadItem::GetUploadedBytes() const {
  return uploaded_bytes_;
}

bool FakeDownloadItem::CanShowInFolder() {
  NOTREACHED_IN_MIGRATION();
  return true;
}

bool FakeDownloadItem::CanOpenDownload() {
  NOTREACHED_IN_MIGRATION();
  return true;
}

bool FakeDownloadItem::ShouldOpenFileBasedOnExtension() {
  NOTREACHED_IN_MIGRATION();
  return true;
}

bool FakeDownloadItem::ShouldOpenFileByPolicyBasedOnExtension() {
  NOTREACHED_IN_MIGRATION();
  return true;
}

bool FakeDownloadItem::GetAutoOpened() {
  NOTREACHED_IN_MIGRATION();
  return false;
}

bool FakeDownloadItem::GetOpened() const {
  NOTREACHED_IN_MIGRATION();
  return false;
}

void FakeDownloadItem::OnContentCheckCompleted(
    download::DownloadDangerType danger_type,
    download::DownloadInterruptReason reason) {
  NOTREACHED_IN_MIGRATION();
}

void FakeDownloadItem::SetOpened(bool opened) {
  NOTREACHED_IN_MIGRATION();
}

void FakeDownloadItem::SetDisplayName(const base::FilePath& name) {
  NOTREACHED_IN_MIGRATION();
}

std::string FakeDownloadItem::DebugString(bool verbose) const {
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

void FakeDownloadItem::SimulateErrorForTesting(
    download::DownloadInterruptReason reason) {
  NOTREACHED_IN_MIGRATION();
}

}  // namespace content
