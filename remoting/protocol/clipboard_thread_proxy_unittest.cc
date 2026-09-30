// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/clipboard_thread_proxy.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/task_environment.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "remoting/protocol/test_event_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting::protocol {

namespace {

using test::EqualsClipboardEvent;
using ::testing::_;

ClipboardEvent MakeClipboardEvent(const std::string& mime_type,
                                  const std::string& data) {
  ClipboardEvent event;
  event.set_mime_type(mime_type);
  event.set_data(data);
  return event;
}

}  // namespace

class ClipboardThreadProxyTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  MockClipboardStub clipboard_stub_;
  base::WeakPtrFactory<ClipboardStub> clipboard_stub_factory_{&clipboard_stub_};
  ClipboardThreadProxy proxy_{clipboard_stub_factory_.GetWeakPtr(),
                              base::SequencedTaskRunner::GetCurrentDefault()};
};

TEST_F(ClipboardThreadProxyTest, InjectsDirectlyOnSameSequence) {
  EXPECT_CALL(clipboard_stub_,
              InjectClipboardEvent(EqualsClipboardEvent("text", "foo")));

  proxy_.InjectClipboardEvent(MakeClipboardEvent("text", "foo"));

  // The event must have been delivered synchronously.
  testing::Mock::VerifyAndClearExpectations(&clipboard_stub_);
}

TEST_F(ClipboardThreadProxyTest, PostsFromOtherSequence) {
  scoped_refptr<base::SequencedTaskRunner> main_task_runner =
      base::SequencedTaskRunner::GetCurrentDefault();
  base::RunLoop run_loop;
  EXPECT_CALL(clipboard_stub_,
              InjectClipboardEvent(EqualsClipboardEvent("text", "foo")))
      .WillOnce([&](const ClipboardEvent&) {
        EXPECT_TRUE(main_task_runner->RunsTasksInCurrentSequence());
        run_loop.Quit();
      });

  base::ThreadPool::PostTask(
      FROM_HERE, base::BindOnce(&ClipboardThreadProxy::InjectClipboardEvent,
                                base::Unretained(&proxy_),
                                MakeClipboardEvent("text", "foo")));
  run_loop.Run();
}

TEST_F(ClipboardThreadProxyTest, DropsPostedEventAfterStubInvalidated) {
  EXPECT_CALL(clipboard_stub_, InjectClipboardEvent(_)).Times(0);

  base::ThreadPool::PostTask(
      FROM_HERE, base::BindOnce(&ClipboardThreadProxy::InjectClipboardEvent,
                                base::Unretained(&proxy_),
                                MakeClipboardEvent("text", "foo")));
  // Wait for the event to be posted to the main sequence, then invalidate the
  // stub before the posted task runs.
  base::ThreadPoolInstance::Get()->FlushForTesting();
  clipboard_stub_factory_.InvalidateWeakPtrs();

  // Tasks on the main sequence run in order, so the posted event has been
  // processed once this quits.
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace remoting::protocol
