// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_TEST_UTILS_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_TEST_UTILS_H_

#include <memory>

#include "base/command_line.h"
#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "components/services/storage/public/mojom/service_worker_storage_control.mojom.h"
#include "content/browser/service_worker/service_worker_cache_writer.h"
#include "content/browser/service_worker/service_worker_host.h"
#include "content/browser/service_worker/service_worker_single_script_update_checker.h"
#include "content/common/navigation_client.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/completion_once_callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace blink {
class StorageKey;
}  // namespace blink

namespace content {

class EmbeddedWorkerTestHelper;
class ScopedServiceWorkerClient;
class ServiceWorkerClient;
class ServiceWorkerContainerHost;
class ServiceWorkerContext;
class ServiceWorkerHost;
class ServiceWorkerRegistration;
class ServiceWorkerRegistry;
class ServiceWorkerVersion;

base::OnceCallback<void(blink::ServiceWorkerStatusCode)>
ReceiveServiceWorkerStatus(std::optional<blink::ServiceWorkerStatusCode>* out,
                           base::OnceClosure quit_closure);

blink::ServiceWorkerStatusCode WarmUpServiceWorker(
    ServiceWorkerVersion* version);

bool WarmUpServiceWorker(ServiceWorkerContext& service_worker_context,
                         const GURL& url);

blink::ServiceWorkerStatusCode StartServiceWorker(
    ServiceWorkerVersion* version);

void StopServiceWorker(ServiceWorkerVersion* version);

// A smart pointer of a committed `ServiceWorkerClient`, used for tests
// involving `ServiceWorkerContainerHost`. The underlying `ServiceWorkerClient`
// is kept alive until `this` is destroyed or `host_remote()` is closed.
class CommittedServiceWorkerClient final {
 public:
  // For Window client: emulate the navigation commit for the service worker
  // client and takes the keep-aliveness of `ServiceWorkerClient`.
  CommittedServiceWorkerClient(
      ScopedServiceWorkerClient service_worker_client,
      const GlobalRenderFrameHostId& render_frame_host_id);

  // For Worker client.
  explicit CommittedServiceWorkerClient(
      ScopedServiceWorkerClient service_worker_client);

  CommittedServiceWorkerClient(CommittedServiceWorkerClient&& other);
  CommittedServiceWorkerClient& operator=(
      CommittedServiceWorkerClient&& other) = delete;

  CommittedServiceWorkerClient(const CommittedServiceWorkerClient&) = delete;
  CommittedServiceWorkerClient& operator=(const CommittedServiceWorkerClient&) =
      delete;

  ~CommittedServiceWorkerClient();

  const base::WeakPtr<ServiceWorkerClient>& AsWeakPtr() const {
    return service_worker_client_;
  }
  ServiceWorkerClient* get() const { return service_worker_client_.get(); }
  ServiceWorkerClient* operator->() const {
    return service_worker_client_.get();
  }

  ServiceWorkerContainerHost& container_host() const;

  // NOTE: These pipes are usable only for Window clients, because for workers
  // the mojo call is not emulated and thus the associated mojo pipes here don't
  // have associated connections.
  mojo::AssociatedRemote<blink::mojom::ServiceWorkerContainerHost>&
  host_remote() {
    return host_remote_;
  }
  mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainer>
  TakeClientReceiver() {
    return std::move(client_receiver_);
  }

 private:
  // Connects to a fake navigation client and keeps alive the message pipe on
  // which |host_remote_| and |client_receiver_| are associated so that they
  // are usable. This is only for navigations. For service workers we can also
  // do the same thing by establishing a
  // blink::mojom::EmbeddedWorkerInstanceClient connection if in the future we
  // really need to make |host_remote_| and |client_receiver_| usable for it.
  mojo::Remote<mojom::NavigationClient> navigation_client_;
  // Bound with content::ServiceWorkerContainerHost. The container host will be
  // removed asynchronously when this remote is closed.
  mojo::AssociatedRemote<blink::mojom::ServiceWorkerContainerHost> host_remote_;
  // This is the other end of
  // mojo::PendingAssociatedRemote<ServiceWorkerContainer> owned by
  // content::ServiceWorkerContainerHost.
  mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainer>
      client_receiver_;

  base::WeakPtr<ServiceWorkerClient> service_worker_client_;
};

// Creates an uncommitted service worker client.
// For clients/ServiceWorkerContainerHost that finished navigation, use
// `CommittedServiceWorkerClient`.
ScopedServiceWorkerClient CreateServiceWorkerClient(
    ServiceWorkerContextCore* context,
    const GURL& document_url,
    const url::Origin& top_frame_origin,
    bool are_ancestors_secure = true,
    FrameTreeNodeId frame_tree_node_id = FrameTreeNodeId(1));
ScopedServiceWorkerClient CreateServiceWorkerClient(
    ServiceWorkerContextCore* context,
    const GURL& document_url,
    bool are_ancestors_secure = true,
    FrameTreeNodeId frame_tree_node_id = FrameTreeNodeId());
ScopedServiceWorkerClient CreateServiceWorkerClient(
    ServiceWorkerContextCore* context,
    bool are_ancestors_secure = true,
    FrameTreeNodeId frame_tree_node_id = FrameTreeNodeId());

std::unique_ptr<ServiceWorkerHost> CreateServiceWorkerHost(
    int process_id,
    bool is_parent_frame_secure,
    ServiceWorkerVersion& hosted_version,
    base::WeakPtr<ServiceWorkerContextCore> context);

// Calls CreateNewRegistration() synchronously.
scoped_refptr<ServiceWorkerRegistration> CreateNewServiceWorkerRegistration(
    ServiceWorkerRegistry* registry,
    const blink::mojom::ServiceWorkerRegistrationOptions& options,
    const blink::StorageKey& key);

// Calls CreateNewVersion() synchronously.
scoped_refptr<ServiceWorkerVersion> CreateNewServiceWorkerVersion(
    ServiceWorkerRegistry* registry,
    scoped_refptr<ServiceWorkerRegistration> registration,
    const GURL& script_url,
    blink::mojom::ScriptType script_type);

// Creates a registration with a waiting version in INSTALLED state.
// |resource_id| is used as ID to represent script resource (|script|) and
// should be unique for each test.
scoped_refptr<ServiceWorkerRegistration>
CreateServiceWorkerRegistrationAndVersion(ServiceWorkerContextCore* context,
                                          const GURL& scope,
                                          const GURL& script,
                                          const blink::StorageKey& key,
                                          int64_t resource_id);

// Writes the script down to |storage| synchronously. This should not be used in
// base::RunLoop since base::RunLoop is used internally to wait for completing
// all of tasks. If it's in another base::RunLoop, consider to use
// WriteToDiskCacheAsync().
storage::mojom::ServiceWorkerResourceRecordPtr WriteToDiskCacheWithIdSync(
    mojo::Remote<storage::mojom::ServiceWorkerStorageControl>& storage,
    const GURL& script_url,
    int64_t resource_id,
    const std::vector<std::pair<std::string, std::string>>& headers,
    const std::string& body,
    const std::string& meta_data);

// Similar to WriteToDiskCacheWithIdSync() but instead of taking a resource id,
// this assigns a new resource ID internally.
storage::mojom::ServiceWorkerResourceRecordPtr WriteToDiskCacheSync(
    mojo::Remote<storage::mojom::ServiceWorkerStorageControl>& storage,
    const GURL& script_url,
    const std::vector<std::pair<std::string, std::string>>& headers,
    const std::string& body,
    const std::string& meta_data);

using WriteToDiskCacheCallback = base::OnceCallback<void(
    storage::mojom::ServiceWorkerResourceRecordPtr record)>;

// Writes the script down to |storage| asynchronously. When completing tasks,
// |callback| will be called. You must wait for |callback| instead of
// base::RunUntilIdle because wiriting to the storage might happen on another
// thread and base::RunLoop could get idle before writes has not finished yet.
void WriteToDiskCacheAsync(
    mojo::Remote<storage::mojom::ServiceWorkerStorageControl>& storage,
    const GURL& script_url,
    const std::vector<std::pair<std::string, std::string>>& headers,
    const std::string& body,
    const std::string& meta_data,
    WriteToDiskCacheCallback callback);

// Calls ServiceWorkerStorageControl::GetNewResourceId() synchronously.
int64_t GetNewResourceIdSync(
    mojo::Remote<storage::mojom::ServiceWorkerStorageControl>& storage);

// A test implementation of ServiceWorkerResourceReader.
//
// This class exposes the ability to expect reads (see ExpectRead*() below).
// Each call to ReadResponseHead() or ReadData() consumes another expected read,
// in the order those reads were expected, so:
//    reader->ExpectReadResponseHeadOk(5);
//    reader->ExpectReadDataOk("abcdef");
//    reader->ExpectReadDataOk("ghijkl");
// Expects these calls, in this order:
//    reader->ReadResponseHead(...);  // reader writes 5 into
//                                    // |response_head->content_length|
//    reader->PrepareReadData();
//    reader->ReadData(...);          // reader writes "abcdef" into |buf|
//    reader->PrepareReadData();
//    reader->ReadData(...);          // reader writes "ghijkl" into |buf|
// If an unexpected call happens, this class DCHECKs.
// An expected read will not complete immediately. It  must be completed by the
// test using CompletePendingRead(). These is a convenience method
// AllExpectedReadsDone() which returns whether there are any expected reads
// that have not yet happened.
class MockServiceWorkerResourceReader
    : public storage::mojom::ServiceWorkerResourceReader {
 public:
  MockServiceWorkerResourceReader();

  MockServiceWorkerResourceReader(const MockServiceWorkerResourceReader&) =
      delete;
  MockServiceWorkerResourceReader& operator=(
      const MockServiceWorkerResourceReader&) = delete;

  ~MockServiceWorkerResourceReader() override;

  mojo::PendingRemote<storage::mojom::ServiceWorkerResourceReader>
  BindNewPipeAndPassRemote(base::OnceClosure disconnect_handler);

  // storage::mojom::ServiceWorkerResourceReader overrides:
  void ReadResponseHead(
      storage::mojom::ServiceWorkerResourceReader::ReadResponseHeadCallback
          callback) override;
  void PrepareReadData(int64_t, PrepareReadDataCallback callback) override;
  void ReadData(ReadDataCallback callback) override;

  // Test helpers. ExpectReadResponseHead() and ExpectReadData() give precise
  // control over both the data to be written and the result to return.
  // ExpectReadResponseHeadOk() and ExpectReadDataOk() are convenience functions
  // for expecting successful reads, which always have their length as their
  // result.

  // Expect a call to ReadResponseHead() on this reader. For these functions,
  // |len| will be used as |response_data_size|, not as the length of this
  // particular read.
  void ExpectReadResponseHead(size_t len, int result);
  void ExpectReadResponseHeadOk(size_t len);

  // Expect a call to ReadData() on this reader. For these functions, |len| is
  // the length of the data to be written back; in ExpectReadDataOk(), |len| is
  // implicitly the length of |data|.
  void ExpectReadData(const char* data, size_t len, int result);
  void ExpectReadDataOk(const std::string& data);

  // Convenient method for calling ExpectReadResponseHeadOk() with the length
  // being |bytes_stored|, and ExpectReadDataOk() for each element of
  // |stored_data|.
  void ExpectReadOk(const std::vector<std::string>& stored_data,
                    const size_t bytes_stored);

  // Complete a pending async read. It is an error to call this function without
  // a pending read (ie, a previous call to ReadResponseHead() or ReadData()
  // having not run its callback yet).
  void CompletePendingRead();

  // Returns whether all expected reads have occurred.
  bool AllExpectedReadsDone() { return expected_reads_.size() == 0; }

 private:
  struct ExpectedRead {
    ExpectedRead(size_t len, int result)
        : data(nullptr), len(len), is_head(true), result(result) {}
    ExpectedRead(const char* data, size_t len, int result)
        : data(data), len(len), is_head(false), result(result) {}
    const char* data;
    size_t len;
    bool is_head;
    int result;
  };

  base::queue<ExpectedRead> expected_reads_;
  size_t expected_max_data_bytes_ = 0;

  mojo::Receiver<storage::mojom::ServiceWorkerResourceReader> receiver_{this};
  storage::mojom::ServiceWorkerResourceReader::ReadResponseHeadCallback
      pending_read_response_head_callback_;
  storage::mojom::ServiceWorkerResourceReader::ReadDataCallback
      pending_read_data_callback_;
  mojo::ScopedDataPipeProducerHandle body_;
};

// A test implementation of ServiceWorkerResourceWriter.
//
// This class exposes the ability to expect writes (see ExpectWrite*Ok() below).
// Each write to this class via WriteResponseHead() or WriteData() consumes
// another expected write, in the order they were added, so:
//   writer->ExpectWriteResponseHeadOk(5);
//   writer->ExpectWriteDataOk(6);
//   writer->ExpectWriteDataOk(6);
// Expects these calls, in this order:
//   writer->WriteResponseHead(...);  // checks that
//                                    // |response_head->content_length| == 5
//   writer->WriteData(...);  // checks that 6 bytes are being written
//   writer->WriteData(...);  // checks that another 6 bytes are being written
// If this class receives an unexpected call to WriteResponseHead() or
// WriteData(), it DCHECKs.
// Expected writes do not complete synchronously, but rather return without
// running their callback and need to be completed with CompletePendingWrite().
// A convenience method AllExpectedWritesDone() is exposed so tests can ensure
// that all expected writes have been consumed by matching calls to WriteInfo()
// or WriteData().
class MockServiceWorkerResourceWriter
    : public storage::mojom::ServiceWorkerResourceWriter {
 public:
  MockServiceWorkerResourceWriter();

  MockServiceWorkerResourceWriter(const MockServiceWorkerResourceWriter&) =
      delete;
  MockServiceWorkerResourceWriter& operator=(
      const MockServiceWorkerResourceWriter&) = delete;

  ~MockServiceWorkerResourceWriter() override;

  mojo::PendingRemote<storage::mojom::ServiceWorkerResourceWriter>
  BindNewPipeAndPassRemote(base::OnceClosure disconnect_handler);

  // ServiceWorkerResourceWriter overrides:
  void WriteResponseHead(network::mojom::URLResponseHeadPtr response_head,
                         WriteResponseHeadCallback callback) override;
  void WriteData(mojo_base::BigBuffer data,
                 WriteDataCallback callback) override;

  // Enqueue expected writes.
  void ExpectWriteResponseHeadOk(size_t len);
  void ExpectWriteResponseHead(size_t len, int result);
  void ExpectWriteDataOk(size_t len);
  void ExpectWriteData(size_t len, int result);

  // Complete a pending asynchronous write. This method DCHECKs unless there is
  // a pending write (a write for which WriteResponseHead() or WriteData() has
  // been called but the callback has not yet been run).
  void CompletePendingWrite();

  // Returns whether all expected reads have been consumed.
  bool AllExpectedWritesDone() { return expected_writes_.size() == 0; }

 private:
  struct ExpectedWrite {
    ExpectedWrite(bool is_head, size_t length, int result)
        : is_head(is_head), length(length), result(result) {}
    bool is_head;
    size_t length;
    int result;
  };

  base::queue<ExpectedWrite> expected_writes_;

  size_t head_written_ = 0;
  size_t data_written_ = 0;

  net::CompletionOnceCallback pending_callback_;

  mojo::Receiver<storage::mojom::ServiceWorkerResourceWriter> receiver_{this};
};

class ServiceWorkerUpdateCheckTestUtils {
 public:
  ServiceWorkerUpdateCheckTestUtils();
  ~ServiceWorkerUpdateCheckTestUtils();

  // Creates a cache writer in the paused state (a difference was found between
  // the old and new script data). |bytes_compared| is the length compared
  // until the difference was found. |new_headers| is the new script's headers.
  // |pending_network_buffer| is a buffer that has the first block of new script
  // data that differs from the old data. |concumsed_size| is the number of
  // bytes of the data consumed from the Mojo data pipe kept in
  // |pending_network_buffer|.
  static std::unique_ptr<ServiceWorkerCacheWriter> CreatePausedCacheWriter(
      EmbeddedWorkerTestHelper* worker_test_helper,
      size_t bytes_compared,
      const std::string& new_headers,
      scoped_refptr<network::MojoToNetPendingBuffer> pending_network_buffer,
      uint32_t consumed_size,
      int64_t old_resource_id,
      int64_t new_resource_id);

  static std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker::PausedState>
  CreateUpdateCheckerPausedState(
      std::unique_ptr<ServiceWorkerCacheWriter> cache_writer,
      ServiceWorkerUpdatedScriptLoader::LoaderState network_loader_state,
      ServiceWorkerUpdatedScriptLoader::WriterState body_writer_state,
      scoped_refptr<network::MojoToNetPendingBuffer> pending_network_buffer,
      uint32_t consumed_size);

  static void SetComparedScriptInfoForVersion(
      const GURL& script_url,
      int64_t resource_id,
      ServiceWorkerSingleScriptUpdateChecker::Result compare_result,
      std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker::PausedState>
          paused_state,
      ServiceWorkerVersion* version);

  // This method calls above three methods to create a cache writer, paused
  // state and compared script info. Then set it to the service worker version.
  static void CreateAndSetComparedScriptInfoForVersion(
      const GURL& script_url,
      size_t bytes_compared,
      const std::string& new_headers,
      const std::string& diff_data_block,
      int64_t old_resource_id,
      int64_t new_resource_id,
      EmbeddedWorkerTestHelper* worker_test_helper,
      ServiceWorkerUpdatedScriptLoader::LoaderState network_loader_state,
      ServiceWorkerUpdatedScriptLoader::WriterState body_writer_state,
      ServiceWorkerSingleScriptUpdateChecker::Result compare_result,
      ServiceWorkerVersion* version,
      mojo::ScopedDataPipeProducerHandle* out_body_handle);

  // Returns false if the entry for |resource_id| doesn't exist in the storage.
  // Returns true when response status is "OK" and response body is same as
  // expected if body exists.
  static bool VerifyStoredResponse(
      int64_t resource_id,
      mojo::Remote<storage::mojom::ServiceWorkerStorageControl>& storage,
      const std::string& expected_body);
};

// Reads all data from the given |handle| and returns data as a string.
// This is similar to mojo::BlockingCopyToString() but a bit different. This
// doesn't wait synchronously but keep posting a task when |handle| returns
// MOJO_RESULT_SHOULD_WAIT.
std::string ReadDataPipe(mojo::ScopedDataPipeConsumerHandle handle);

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_TEST_UTILS_H_
