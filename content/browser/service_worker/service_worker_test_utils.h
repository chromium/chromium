// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_TEST_UTILS_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_TEST_UTILS_H_

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "base/task/post_task.h"
#include "content/browser/service_worker/service_worker_database.h"
#include "content/browser/service_worker/service_worker_disk_cache.h"
#include "content/common/service_worker/service_worker_provider.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace net {

class HttpResponseInfo;

}  // namespace net

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerProviderHost;
class ServiceWorkerStorage;
class ServiceWorkerVersion;

template <typename Arg>
void ReceiveResult(BrowserThread::ID run_quit_thread,
                   base::OnceClosure quit,
                   base::Optional<Arg>* out,
                   Arg actual) {
  *out = actual;
  if (!quit.is_null())
    base::PostTaskWithTraits(FROM_HERE, {run_quit_thread}, std::move(quit));
}

template <typename Arg>
base::OnceCallback<void(Arg)> CreateReceiver(BrowserThread::ID run_quit_thread,
                                             base::OnceClosure quit,
                                             base::Optional<Arg>* out) {
  return base::BindOnce(&ReceiveResult<Arg>, run_quit_thread, std::move(quit),
                        out);
}

template <typename Arg>
base::OnceCallback<void(Arg)> CreateReceiverOnCurrentThread(
    base::Optional<Arg>* out,
    base::OnceClosure quit = base::OnceClosure()) {
  BrowserThread::ID id;
  bool ret = BrowserThread::GetCurrentThreadIdentifier(&id);
  DCHECK(ret);
  return CreateReceiver(id, std::move(quit), out);
}

// Container for keeping the Mojo connection to the service worker provider on
// the renderer alive.
class ServiceWorkerRemoteProviderEndpoint {
 public:
  ServiceWorkerRemoteProviderEndpoint();
  ServiceWorkerRemoteProviderEndpoint(
      ServiceWorkerRemoteProviderEndpoint&& other);
  ~ServiceWorkerRemoteProviderEndpoint();

  void BindWithProviderHostInfo(mojom::ServiceWorkerProviderHostInfoPtr* info);
  void BindWithProviderInfo(
      mojom::ServiceWorkerProviderInfoForStartWorkerPtr info);

  mojom::ServiceWorkerContainerHostAssociatedPtr* host_ptr() {
    return &host_ptr_;
  }

  mojom::ServiceWorkerContainerAssociatedRequest* client_request() {
    return &client_request_;
  }

 private:
  // Bound with content::ServiceWorkerProviderHost. The provider host will be
  // removed asynchronously when this pointer is closed.
  mojom::ServiceWorkerContainerHostAssociatedPtr host_ptr_;
  // This is the other end of ServiceWorkerContainerAssociatedPtr owned by
  // content::ServiceWorkerProviderHost.
  mojom::ServiceWorkerContainerAssociatedRequest client_request_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerRemoteProviderEndpoint);
};

mojom::ServiceWorkerProviderHostInfoPtr CreateProviderHostInfoForWindow(
    int provider_id,
    int route_id);

std::unique_ptr<ServiceWorkerProviderHost> CreateProviderHostForWindow(
    int process_id,
    int provider_id,
    bool is_parent_frame_secure,
    base::WeakPtr<ServiceWorkerContextCore> context,
    ServiceWorkerRemoteProviderEndpoint* output_endpoint);

base::WeakPtr<ServiceWorkerProviderHost>
CreateProviderHostForServiceWorkerContext(
    int process_id,
    bool is_parent_frame_secure,
    ServiceWorkerVersion* hosted_version,
    base::WeakPtr<ServiceWorkerContextCore> context,
    ServiceWorkerRemoteProviderEndpoint* output_endpoint);

// Writes the script down to |storage| synchronously. This should not be used in
// base::RunLoop since base::RunLoop is used internally to wait for completing
// all of tasks. If it's in another base::RunLoop, consider to use
// WriteToDiskCacheAsync().
ServiceWorkerDatabase::ResourceRecord WriteToDiskCacheSync(
    ServiceWorkerStorage* storage,
    const GURL& script_url,
    int64_t resource_id,
    const std::vector<std::pair<std::string, std::string>>& headers,
    const std::string& body,
    const std::string& meta_data);

// Writes the script with custom net::HttpResponseInfo down to |storage|
// synchronously. This should not be used in base::RunLoop since base::RunLoop
// is used internally to wait for completing all of tasks. If it's in another
// base::RunLoop, consider to use WriteToDiskCacheWithCustomResponseInfoAsync().
ServiceWorkerDatabase::ResourceRecord
WriteToDiskCacheWithCustomResponseInfoSync(
    ServiceWorkerStorage* storage,
    const GURL& script_url,
    int64_t resource_id,
    std::unique_ptr<net::HttpResponseInfo> http_info,
    const std::string& body,
    const std::string& meta_data);

// Writes the script down to |storage| asynchronously. When completing tasks,
// |callback| will be called. You must wait for |callback| instead of
// base::RunUntilIdle because wiriting to the storage might happen on another
// thread and base::RunLoop could get idle before writes has not finished yet.
ServiceWorkerDatabase::ResourceRecord WriteToDiskCacheAsync(
    ServiceWorkerStorage* storage,
    const GURL& script_url,
    int64_t resource_id,
    const std::vector<std::pair<std::string, std::string>>& headers,
    const std::string& body,
    const std::string& meta_data,
    base::OnceClosure callback);

// Writes the script with custom net::HttpResponseInfo down to |storage|
// asynchronously. When completing tasks, |callback| will be called. You must
// wait for |callback| instead of base::RunUntilIdle because wiriting to the
// storage might happen on another thread and base::RunLoop could get idle
// before writes has not finished yet.
ServiceWorkerDatabase::ResourceRecord
WriteToDiskCacheWithCustomResponseInfoAsync(
    ServiceWorkerStorage* storage,
    const GURL& script_url,
    int64_t resource_id,
    std::unique_ptr<net::HttpResponseInfo> http_info,
    const std::string& body,
    const std::string& meta_data,
    base::OnceClosure callback);

// A test implementation of ServiceWorkerResponseReader.
//
// This class exposes the ability to expect reads (see ExpectRead*() below).
// Each call to ReadInfo() or ReadData() consumes another expected read, in the
// order those reads were expected, so:
//    reader->ExpectReadInfoOk(5, false);
//    reader->ExpectReadDataOk("abcdef", false);
//    reader->ExpectReadDataOk("ghijkl", false);
// Expects these calls, in this order:
//    reader->ReadInfo(...);  // reader writes 5 into
//                            // |info_buf->response_data_size|
//    reader->ReadData(...);  // reader writes "abcdef" into |buf|
//    reader->ReadData(...);  // reader writes "ghijkl" into |buf|
// If an unexpected call happens, this class DCHECKs.
// If an expected read is marked "async", it will not complete immediately, but
// must be completed by the test using CompletePendingRead().
// These is a convenience method AllExpectedReadsDone() which returns whether
// there are any expected reads that have not yet happened.
class MockServiceWorkerResponseReader : public ServiceWorkerResponseReader {
 public:
  MockServiceWorkerResponseReader();
  ~MockServiceWorkerResponseReader() override;

  // ServiceWorkerResponseReader overrides
  void ReadInfo(HttpResponseInfoIOBuffer* info_buf,
                OnceCompletionCallback callback) override;
  void ReadData(net::IOBuffer* buf,
                int buf_len,
                OnceCompletionCallback callback) override;

  // Test helpers. ExpectReadInfo() and ExpectReadData() give precise control
  // over both the data to be written and the result to return.
  // ExpectReadInfoOk() and ExpectReadDataOk() are convenience functions for
  // expecting successful reads, which always have their length as their result.

  // Expect a call to ReadInfo() on this reader. For these functions, |len| will
  // be used as |response_data_size|, not as the length of this particular read.
  void ExpectReadInfo(size_t len, bool async, int result);
  void ExpectReadInfoOk(size_t len, bool async);

  // Expect a call to ReadData() on this reader. For these functions, |len| is
  // the length of the data to be written back; in ExpectReadDataOk(), |len| is
  // implicitly the length of |data|.
  void ExpectReadData(const char* data, size_t len, bool async, int result);
  void ExpectReadDataOk(const std::string& data, bool async);

  // Convenient method for calling ExpectReadInfoOk() with the length being
  // |bytes_stored|, and ExpectReadDataOk() for each element of |stored_data|.
  void ExpectReadOk(const std::vector<std::string>& stored_data,
                    const size_t bytes_stored,
                    const bool async);

  // Complete a pending async read. It is an error to call this function without
  // a pending async read (ie, a previous call to ReadInfo() or ReadData()
  // having not run its callback yet).
  void CompletePendingRead();

  // Returns whether all expected reads have occurred.
  bool AllExpectedReadsDone() { return expected_reads_.size() == 0; }

 private:
  struct ExpectedRead {
    ExpectedRead(size_t len, bool async, int result)
        : data(nullptr), len(len), info(true), async(async), result(result) {}
    ExpectedRead(const char* data, size_t len, bool async, int result)
        : data(data), len(len), info(false), async(async), result(result) {}
    const char* data;
    size_t len;
    bool info;
    bool async;
    int result;
  };

  base::queue<ExpectedRead> expected_reads_;
  scoped_refptr<net::IOBuffer> pending_buffer_;
  size_t pending_buffer_len_;
  scoped_refptr<HttpResponseInfoIOBuffer> pending_info_;
  OnceCompletionCallback pending_callback_;

  DISALLOW_COPY_AND_ASSIGN(MockServiceWorkerResponseReader);
};

// A test implementation of ServiceWorkerResponseWriter.
//
// This class exposes the ability to expect writes (see ExpectWrite*Ok() below).
// Each write to this class via WriteInfo() or WriteData() consumes another
// expected write, in the order they were added, so:
//   writer->ExpectWriteInfoOk(5, false);
//   writer->ExpectWriteDataOk(6, false);
//   writer->ExpectWriteDataOk(6, false);
// Expects these calls, in this order:
//   writer->WriteInfo(...);  // checks that |buf->response_data_size| == 5
//   writer->WriteData(...);  // checks that 6 bytes are being written
//   writer->WriteData(...);  // checks that another 6 bytes are being written
// If this class receives an unexpected call to WriteInfo() or WriteData(), it
// DCHECKs.
// Expected writes marked async do not complete synchronously, but rather return
// without running their callback and need to be completed with
// CompletePendingWrite().
// A convenience method AllExpectedWritesDone() is exposed so tests can ensure
// that all expected writes have been consumed by matching calls to WriteInfo()
// or WriteData().
class MockServiceWorkerResponseWriter : public ServiceWorkerResponseWriter {
 public:
  MockServiceWorkerResponseWriter();
  ~MockServiceWorkerResponseWriter() override;

  // ServiceWorkerResponseWriter overrides
  void WriteInfo(HttpResponseInfoIOBuffer* info_buf,
                 OnceCompletionCallback callback) override;
  void WriteData(net::IOBuffer* buf,
                 int buf_len,
                 OnceCompletionCallback callback) override;

  // Enqueue expected writes.
  void ExpectWriteInfoOk(size_t len, bool async);
  void ExpectWriteInfo(size_t len, bool async, int result);
  void ExpectWriteDataOk(size_t len, bool async);
  void ExpectWriteData(size_t len, bool async, int result);

  // Complete a pending asynchronous write. This method DCHECKs unless there is
  // a pending write (a write for which WriteInfo() or WriteData() has been
  // called but the callback has not yet been run).
  void CompletePendingWrite();

  // Returns whether all expected reads have been consumed.
  bool AllExpectedWritesDone() { return expected_writes_.size() == 0; }

 private:
  struct ExpectedWrite {
    ExpectedWrite(bool is_info, size_t length, bool async, int result)
        : is_info(is_info), length(length), async(async), result(result) {}
    bool is_info;
    size_t length;
    bool async;
    int result;
  };

  base::queue<ExpectedWrite> expected_writes_;

  size_t info_written_;
  size_t data_written_;

  OnceCompletionCallback pending_callback_;

  DISALLOW_COPY_AND_ASSIGN(MockServiceWorkerResponseWriter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_TEST_UTILS_H_
