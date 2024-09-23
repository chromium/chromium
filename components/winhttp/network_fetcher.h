// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WINHTTP_NETWORK_FETCHER_H_
#define COMPONENTS_WINHTTP_NETWORK_FETCHER_H_

#include <windows.h>

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "components/winhttp/proxy_configuration.h"
#include "components/winhttp/scoped_hinternet.h"
#include "components/winhttp/scoped_winttp_proxy_info.h"
#include "url/gurl.h"

namespace base {
class SequencedTaskRunner;
}

namespace winhttp {

// Implements a network fetcher in terms of WinHTTP. The class is ref-counted
// as it is accessed from the main sequence and the worker threads in WinHTTP.
class NetworkFetcher : public base::RefCountedThreadSafe<NetworkFetcher> {
 public:
  using FetchCompleteCallback = base::OnceCallback<void(int response_code)>;
  using FetchStartedCallback =
      base::OnceCallback<void(int response_code, int64_t content_length)>;
  using FetchProgressCallback = base::RepeatingCallback<void(int64_t current)>;

  NetworkFetcher(scoped_refptr<SharedHInternet> session_handle,
                 scoped_refptr<ProxyConfiguration> proxy_configuration);
  NetworkFetcher(const NetworkFetcher&) = delete;
  NetworkFetcher& operator=(const NetworkFetcher&) = delete;

  void Close();

  void PostRequest(
      const GURL& url,
      const std::string& post_data,
      const std::string& content_type,
      const base::flat_map<std::string, std::string>& post_additional_headers,
      FetchStartedCallback fetch_started_callback,
      FetchProgressCallback fetch_progress_callback,
      FetchCompleteCallback fetch_complete_callback);

  // Downloads the content of the |url| to a file identified by |file_path|.
  // The content is written to the file as it is being retrieved from the
  // network. Returns a closure that can be run to cancel the download.
  base::OnceClosure DownloadToFile(
      const GURL& url,
      const base::FilePath& file_path,
      FetchStartedCallback fetch_started_callback,
      FetchProgressCallback fetch_progress_callback,
      FetchCompleteCallback fetch_complete_callback);

  HRESULT QueryHeaderString(const std::wstring& name,
                            std::wstring* value) const;
  HRESULT QueryHeaderInt(const std::wstring& name, int* value) const;
  std::string GetResponseBody() const;
  HRESULT GetNetError() const;
  base::FilePath GetFilePath() const;

  // Returns the number of bytes retrieved from the network. This may be
  // different than the content length if an error occurred.
  int64_t GetContentSize() const;

 private:
  friend class base::RefCountedThreadSafe<NetworkFetcher>;
  using WriteDataCallback = base::RepeatingCallback<void()>;

  ~NetworkFetcher();

  static void __stdcall WinHttpStatusCallback(HINTERNET handle,
                                              DWORD_PTR context,
                                              DWORD status,
                                              void* info,
                                              DWORD info_len);

  // Invoked by the last WinHTTPstatus status callback.
  void HandleClosing();

  DWORD_PTR context() const { return reinterpret_cast<DWORD_PTR>(this); }

  HRESULT BeginFetch(
      const std::string& data,
      const base::flat_map<std::string, std::string>& additional_headers);
  std::optional<ScopedWinHttpProxyInfo> GetProxyForUrl();
  void ContinueFetch(
      const std::string& data,
      base::flat_map<std::string, std::string> additional_headers,
      std::optional<ScopedWinHttpProxyInfo> winhttp_proxy_info);

  ScopedHInternet Connect();
  ScopedHInternet OpenRequest();
  HRESULT SendRequest(const std::string& data);
  void SendRequestComplete();
  HRESULT ReceiveResponse();
  void HeadersAvailable();
  HRESULT ReadData();
  void ReadDataComplete(size_t num_bytes_read);
  void RequestError(DWORD error);
  void CompleteFetch();

  void WriteDataToMemory();
  void WriteDataToFile();
  bool WriteDataToFileBlocking();
  void WriteDataToFileComplete(bool is_eof);

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  scoped_refptr<SharedHInternet> session_handle_;
  scoped_refptr<ProxyConfiguration> proxy_configuration_;
  ScopedHInternet connect_handle_;
  ScopedHInternet request_handle_;

  // Keeps an outstanding reference count on itself as long as there is a
  // valid request handle and the context for the handle is set to this
  // instance.
  scoped_refptr<NetworkFetcher> self_;

  GURL url_;
  bool is_https_ = false;
  std::string host_;
  int port_ = 0;
  std::string path_for_request_;

  std::wstring_view verb_;
  std::string request_data_;
  // The value of Content-Type header, e.g. "application/json".
  std::string content_type_;
  WriteDataCallback write_data_callback_;
  HRESULT net_error_ = S_OK;
  std::vector<char> read_buffer_;
  int response_code_ = 0;
  std::string post_response_body_;
  base::FilePath file_path_;
  base::File file_;
  int64_t content_size_ = 0;

  FetchStartedCallback fetch_started_callback_;
  FetchProgressCallback fetch_progress_callback_;
  FetchCompleteCallback fetch_complete_callback_;
};

}  // namespace winhttp

#endif  // COMPONENTS_WINHTTP_NETWORK_FETCHER_H_
