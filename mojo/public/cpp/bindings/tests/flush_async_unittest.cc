// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "mojo/public/cpp/bindings/async_flusher.h"
#include "mojo/public/cpp/bindings/pending_flush.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/tests/bindings_test_base.h"
#include "mojo/public/cpp/bindings/tests/flush_async_unittest.test-mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace flush_async_unittest {

// This implementation binds its receiver on an arbitrary ThreadPool task
// runner. Any incoming Writer receivers are in turn bound on arbitrary (and
// potentially different) ThreadPool task runners. There is therefore no general
// ordering guarantee regarding message dispatch among each bound interface,
// yielding generally racy behavior.
//
// This allows tests to reliably verify correctness of async flushing behavior.
class KeyValueStoreImpl : public base::RefCountedThreadSafe<KeyValueStoreImpl>,
                          public mojom::KeyValueStore {
 public:
  KeyValueStoreImpl()
      : task_runner_(base::ThreadPool::CreateSequencedTaskRunner({})) {}

  void Bind(PendingReceiver<mojom::KeyValueStore> receiver) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&KeyValueStoreImpl::BindOnTaskRunner, this,
                                  std::move(receiver)));
  }

  void ShutDown(base::OnceClosure callback) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&KeyValueStoreImpl::ShutDownOnTaskRunner,
                                  this, std::move(callback)));
  }

  void StoreValue(const std::string& key, const std::string& value) {
    base::AutoLock locker(lock_);
    contents_[key] = value;
  }

 private:
  friend class base::RefCountedThreadSafe<KeyValueStoreImpl>;

  class WriterImpl : public mojom::Writer {
   public:
    WriterImpl(KeyValueStoreImpl* key_value_store)
        : task_runner_(base::ThreadPool::CreateSequencedTaskRunner({})),
          key_value_store_(key_value_store) {}
    ~WriterImpl() override = default;

    void Bind(PendingReceiver<mojom::Writer> receiver) {
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&WriterImpl::BindOnTaskRunner, base::Unretained(this),
                         std::move(receiver)));
    }

    void ShutDown(base::OnceClosure callback) {
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&WriterImpl::ShutDownOnTaskRunner,
                         base::Unretained(this), std::move(callback)));
    }

    // mojom::Writer implementation:
    void Put(const std::string& key, const std::string& value) override {
      key_value_store_->StoreValue(key, value);
    }

   private:
    void BindOnTaskRunner(PendingReceiver<mojom::Writer> receiver) {
      receiver_ =
          std::make_unique<Receiver<mojom::Writer>>(this, std::move(receiver));
    }

    void ShutDownOnTaskRunner(base::OnceClosure callback) {
      receiver_.reset();
      std::move(callback).Run();
    }

    const scoped_refptr<base::SequencedTaskRunner> task_runner_;
    const raw_ptr<KeyValueStoreImpl> key_value_store_;
    std::unique_ptr<Receiver<mojom::Writer>> receiver_;
  };

  void BindOnTaskRunner(PendingReceiver<mojom::KeyValueStore> receiver) {
    receiver_ = std::make_unique<Receiver<mojom::KeyValueStore>>(
        this, std::move(receiver));
  }

  void ShutDownOnTaskRunner(base::OnceClosure callback) {
    receiver_.reset();
    client_.reset();

    // Shutdown all WriterImpls too.
    auto shutdown = base::BarrierClosure(writers_.size(), std::move(callback));
    for (auto& writer : writers_)
      writer->ShutDown(base::BindOnce(shutdown));
  }

  // mojom::KeyValueStore implementation:
  void SetClient(PendingRemote<mojom::KeyValueStoreClient> client) override {
    client_.Bind(std::move(client));
  }

  void BindWriter(PendingReceiver<mojom::Writer> receiver) override {
    // NOTE: Each WriterImpl internally binds on an arbitrary ThreadPool task
    // runner, leaving us with no ordering guarantee among Writers with respect
    // to each other or this KeyValueStore.
    auto new_writer = std::make_unique<WriterImpl>(this);
    new_writer->Bind(std::move(receiver));
    writers_.push_back(std::move(new_writer));
  }

  void GetSnapshot(GetSnapshotCallback callback) override {
    base::AutoLock locker(lock_);
    std::move(callback).Run(contents_);

    // If we have a client, notify it that a snapshot was taken, but ensure that
    // it doesn't dispatch that notification until the above callback is
    // dispatched. Then also ensure that our remote doesn't receiver any
    // subsequent callbacks until the client processes this |OnSnapshotTaken()|.
    if (client_) {
      client_.PauseReceiverUntilFlushCompletes(receiver_->FlushAsync());
      client_->OnSnapshotTaken();
      receiver_->PauseRemoteCallbacksUntilFlushCompletes(client_.FlushAsync());
    }
  }

  ~KeyValueStoreImpl() override = default;

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<Receiver<mojom::KeyValueStore>> receiver_;
  Remote<mojom::KeyValueStoreClient> client_;
  std::vector<std::unique_ptr<WriterImpl>> writers_;
  base::Lock lock_;
  base::flat_map<std::string, std::string> contents_;
};

class FlushAsyncTest : public BindingsTestBase {
 public:
  FlushAsyncTest() {
    key_value_store_->Bind(
        remote_key_value_store_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    base::RunLoop wait_for_clean_shutdown;
    key_value_store_->ShutDown(wait_for_clean_shutdown.QuitClosure());
    wait_for_clean_shutdown.Run();
  }

  Remote<mojom::KeyValueStore>& key_value_store() {
    return remote_key_value_store_;
  }

  Remote<mojom::Writer> MakeWriter() {
    Remote<mojom::Writer> writer;
    key_value_store()->BindWriter(writer.BindNewPipeAndPassReceiver());
    return writer;
  }

 private:
  Remote<mojom::KeyValueStore> remote_key_value_store_;
  scoped_refptr<KeyValueStoreImpl> key_value_store_{
      base::MakeRefCounted<KeyValueStoreImpl>()};
};

TEST_P(FlushAsyncTest, WaitForMultipleFlushes) {
  const std::string kKey1 = "bar";
  const std::string kKey2 = "foo";
  const std::string kValue1 = "42";
  const std::string kValue2 = "37";

  Remote<mojom::Writer> writer1 = MakeWriter();
  Remote<mojom::Writer> writer2 = MakeWriter();
  writer1->Put(kKey1, kValue1);
  writer2->Put(kKey2, kValue2);

  // Both |Put()| calls must be received by the time |GetSnapshot()| is
  // dispatched.
  base::flat_map<std::string, std::string> snapshot;
  base::RunLoop loop;
  key_value_store().PauseReceiverUntilFlushCompletes(writer1.FlushAsync());
  key_value_store().PauseReceiverUntilFlushCompletes(writer2.FlushAsync());
  key_value_store()->GetSnapshot(base::BindLambdaForTesting(
      [&](const base::flat_map<std::string, std::string>& contents) {
        snapshot = contents;
        loop.Quit();
      }));
  loop.Run();

  EXPECT_EQ(2u, snapshot.size());
  EXPECT_EQ(kValue1, snapshot[kKey1]);
  EXPECT_EQ(kValue2, snapshot[kKey2]);
}

TEST_P(FlushAsyncTest, MultipleFlushesInSequence) {
  const std::string kKey1 = "foo";
  const std::string kKey2 = "bar";
  const std::string kKey3 = "baz";
  const std::string kValue1 = "1";
  const std::string kValue2 = "2";
  const std::string kValue3 = "3";

  Remote<mojom::Writer> writer1 = MakeWriter();
  Remote<mojom::Writer> writer2 = MakeWriter();
  writer1->Put(kKey1, kValue1);
  writer1.FlushForTesting();

  // Pause each Writer until the |GetSnapshot()| call below has executed,
  // ensuring that the snapshot never reflects the result of the |Put()| calls
  // below.
  base::RunLoop loop;
  base::flat_map<std::string, std::string> snapshot;
  key_value_store()->GetSnapshot(base::BindLambdaForTesting(
      [&](const base::flat_map<std::string, std::string>& contents) {
        snapshot = contents;
        loop.Quit();
      }));
  writer1.PauseReceiverUntilFlushCompletes(key_value_store().FlushAsync());
  writer2.PauseReceiverUntilFlushCompletes(key_value_store().FlushAsync());
  writer1->Put(kKey2, kValue2);
  writer2->Put(kKey3, kValue3);
  loop.Run();

  EXPECT_EQ(1u, snapshot.size());
  EXPECT_EQ(kValue1, snapshot[kKey1]);
}

TEST_P(FlushAsyncTest, DroppedFlusherCompletesPendingFlush) {
  const std::string kKey = "foo";
  const std::string kValue = "bar";
  Remote<mojom::Writer> writer = MakeWriter();
  writer->Put(kKey, kValue);
  writer.FlushForTesting();

  // Pause the KeyValueStore to block |GetSnapshot()|, but drop the
  // corresponding AsyncFlusher. The call should eventually execute.
  base::RunLoop loop;
  base::flat_map<std::string, std::string> snapshot;
  std::optional<AsyncFlusher> flusher(std::in_place);
  key_value_store().PauseReceiverUntilFlushCompletes(
      PendingFlush(&flusher.value()));
  key_value_store()->GetSnapshot(base::BindLambdaForTesting(
      [&](const base::flat_map<std::string, std::string>& contents) {
        snapshot = contents;
        loop.Quit();
      }));
  flusher.reset();
  loop.Run();

  EXPECT_EQ(1u, snapshot.size());
  EXPECT_EQ(kValue, snapshot[kKey]);
}

class PingerImpl : public mojom::Pinger {
 public:
  explicit PingerImpl(PendingReceiver<mojom::Pinger> receiver)
      : receiver_(this, std::move(receiver)) {}
  ~PingerImpl() override = default;

  Receiver<mojom::Pinger>& receiver() { return receiver_; }

  // mojom::Pinger implementation:
  void Ping(PingCallback callback) override { std::move(callback).Run(); }

 private:
  Receiver<mojom::Pinger> receiver_;
};

TEST_P(FlushAsyncTest, PausedInterfaceDoesNotAutoResumeOnFlush) {
  // Verifies that if a receiver is implicitly paused via a remote call to
  // |PauseReceiverUntilFlushCompletes()|, but also explicitly paused by its
  // owner calling |Pause()|, it does not auto-resume when the flush completes.

  Remote<mojom::Pinger> pinger;
  PingerImpl impl(pinger.BindNewPipeAndPassReceiver());

  std::optional<AsyncFlusher> flusher(std::in_place);
  PendingFlush flush(&flusher.value());
  pinger.PauseReceiverUntilFlushCompletes(std::move(flush));

  // Allow the receiver to become implicitly paused as a result of the above
  // call. Using |RunUntilIdle()| is safe here since this is a simple unit test
  // and we are only concerned with activity on the calling sequence.
  base::RunLoop().RunUntilIdle();

  // We should not see a reply until the receiver is unpaused.
  bool got_reply = false;
  base::RunLoop ping_loop;
  pinger->Ping(base::BindLambdaForTesting([&] {
    ping_loop.Quit();
    got_reply = true;
  }));

  // Explicitly pause the receiver and complete the AsyncFlusher.
  impl.receiver().Pause();
  flusher.reset();

  // Ensure that any asynchronous side-effects of resetting the AsyncFlusher
  // have a chance to execute.
  base::RunLoop().RunUntilIdle();

  // The receiver should still be paused despite the flush completing, because
  // we haven't called an explicit |Resume()| to match the explicit |Pause()|
  // above.
  EXPECT_FALSE(got_reply);

  // Now allow it to resume and verify that everything's cool.
  impl.receiver().Resume();
  ping_loop.Run();
  EXPECT_TRUE(got_reply);
}

TEST_P(FlushAsyncTest, ResumeDoesNotInterruptWaitingOnFlush) {
  // Verifies that an explicit |Resume()| does not actually resume message
  // processing if the endpoint is still waiting on an asynchronous flush
  // operation.

  Remote<mojom::Pinger> pinger;
  PingerImpl impl(pinger.BindNewPipeAndPassReceiver());

  std::optional<AsyncFlusher> flusher(std::in_place);
  PendingFlush flush(&flusher.value());
  pinger.PauseReceiverUntilFlushCompletes(std::move(flush));

  // Allow the receiver to become implicitly paused as a result of the above
  // call. Using |RunUntilIdle()| is safe here since this is a simple unit test
  // and we are only concerned with activity on the calling sequence.
  base::RunLoop().RunUntilIdle();

  // We should not see a reply until the receiver is unpaused.
  bool got_reply = false;
  base::RunLoop ping_loop;
  pinger->Ping(base::BindLambdaForTesting([&] {
    ping_loop.Quit();
    got_reply = true;
  }));

  // Explicitly resume the receiver and let tasks settle. There should still be
  // no reply, because |flusher| is still active and the receiver is waiting on
  // it to complete.
  impl.receiver().Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(got_reply);

  // Now allow the flush to complete and verify that the receiver is unblocked.
  flusher.reset();
  ping_loop.Run();
  EXPECT_TRUE(got_reply);
}

class KeyValueStoreClientImpl : public mojom::KeyValueStoreClient {
 public:
  explicit KeyValueStoreClientImpl(
      PendingReceiver<mojom::KeyValueStoreClient> receiver)
      : receiver_(this, std::move(receiver)) {}
  ~KeyValueStoreClientImpl() override = default;

  Receiver<mojom::KeyValueStoreClient>& receiver() { return receiver_; }

  void set_snapshot_taken_callback(base::RepeatingClosure callback) {
    snapshot_taken_callback_ = std::move(callback);
  }

  // mojom::KeyValueStoreClient implementation:
  void OnSnapshotTaken() override {
    if (snapshot_taken_callback_)
      snapshot_taken_callback_.Run();
  }

 private:
  Receiver<mojom::KeyValueStoreClient> receiver_;
  base::RepeatingClosure snapshot_taken_callback_;
};

TEST_P(FlushAsyncTest, PauseRemote) {
  // Smoke test to exercise the async flushing APIs on a Receiver to pause
  // callback dispatch on its corresponding Remote. |GetSnapshot()| replies are
  // strictly ordered against corresponding calls to |OnSnapshotTaken()| on the
  // client interface. This is enforced entirely by logic in
  // |KeyValueStoreImpl::GetSnapshot()| using async flush operations.

  PendingRemote<mojom::KeyValueStoreClient> client;
  KeyValueStoreClientImpl impl(client.InitWithNewPipeAndPassReceiver());
  key_value_store()->SetClient(std::move(client));

  int num_replies = 0;
  int num_client_calls = 0;

  // Any time the client gets an |OnSnapshotTaken()| call, it should be able
  // to rely on the corresponding |GetSnapshot()| reply having already been
  // dispatched.
  impl.set_snapshot_taken_callback(base::BindLambdaForTesting([&] {
    EXPECT_EQ(num_replies, num_client_calls + 1);
    ++num_client_calls;
  }));

  // Perform a few trial snapshots. All replies should be dispatched after any
  // previous snapshot's client notification, but before its own corresponding
  // client notification.
  base::RunLoop loop;
  key_value_store()->GetSnapshot(base::BindLambdaForTesting(
      [&](const base::flat_map<std::string, std::string>&) {
        EXPECT_EQ(0, num_replies);
        EXPECT_EQ(0, num_client_calls);
        ++num_replies;
      }));
  key_value_store()->GetSnapshot(base::BindLambdaForTesting(
      [&](const base::flat_map<std::string, std::string>&) {
        EXPECT_EQ(1, num_replies);
        EXPECT_EQ(1, num_client_calls);
        ++num_replies;
      }));
  key_value_store()->GetSnapshot(base::BindLambdaForTesting(
      [&](const base::flat_map<std::string, std::string>&) {
        EXPECT_EQ(2, num_replies);
        EXPECT_EQ(2, num_client_calls);
        ++num_replies;
        loop.Quit();
      }));
  loop.Run();
}

INSTANTIATE_MOJO_BINDINGS_TEST_SUITE_P(FlushAsyncTest);

}  // namespace flush_async_unittest
}  // namespace test
}  // namespace mojo
