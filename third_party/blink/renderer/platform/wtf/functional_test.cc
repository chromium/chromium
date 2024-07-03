/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/wtf/functional.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/gtest_util.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/leak_annotations.h"

namespace WTF {

class HasWeakPtrSupport {
 public:
  HasWeakPtrSupport() {}

  base::WeakPtr<HasWeakPtrSupport> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void RevokeAll() { weak_ptr_factory_.InvalidateWeakPtrs(); }

  void Increment(int* counter) { ++*counter; }

 private:
  base::WeakPtrFactory<HasWeakPtrSupport> weak_ptr_factory_{this};
};

}  // namespace WTF

namespace WTF {
namespace {

TEST(FunctionalTest, WeakPtr) {
  HasWeakPtrSupport obj;
  int counter = 0;
  base::RepeatingClosure bound =
      WTF::BindRepeating(&HasWeakPtrSupport::Increment, obj.GetWeakPtr(),
                         WTF::Unretained(&counter));

  bound.Run();
  EXPECT_FALSE(bound.IsCancelled());
  EXPECT_EQ(1, counter);

  obj.RevokeAll();
  EXPECT_TRUE(bound.IsCancelled());
  bound.Run();
  EXPECT_EQ(1, counter);
}

int PingPong(int* i_ptr) {
  return *i_ptr;
}

TEST(FunctionalTest, RawPtr) {
  int i = 123;
  raw_ptr<int> p = &i;

  auto callback = WTF::BindRepeating(PingPong, WTF::Unretained(p));
  int res = callback.Run();
  EXPECT_EQ(123, res);
}

void MakeClosure(base::OnceClosure** closure_out) {
  *closure_out = new base::OnceClosure(WTF::BindOnce([] {}));
  LEAK_SANITIZER_IGNORE_OBJECT(*closure_out);
}

TEST(FunctionalTest, ThreadRestriction) {
  base::OnceClosure* closure = nullptr;

  base::Thread thread("testing");
  thread.Start();
  thread.task_runner()->PostTask(
      FROM_HERE, ConvertToBaseOnceCallback(CrossThreadBindOnce(
                     &MakeClosure, CrossThreadUnretained(&closure))));
  thread.Stop();

  ASSERT_TRUE(closure);
  EXPECT_DCHECK_DEATH(std::move(*closure).Run());
  EXPECT_DCHECK_DEATH(delete closure);
}

}  // namespace
}  // namespace WTF
