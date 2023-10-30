// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ptr_util.h"
#include "base/test/bind.h"
#include "storage/browser/file_system/copy_or_move_hook_delegate.h"
#include "storage/browser/file_system/copy_or_move_hook_delegate_composite.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/test/mock_copy_or_move_hook_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace storage {

namespace {
size_t GetLeadingOK(std::vector<base::File::Error> status) {
  size_t i;
  for (i = 0; i < status.size() && status[i] == base::File::FILE_OK; ++i)
    ;
  return i;
}
}  // namespace

struct TestParameter {
  // Error / status returned by the different instances of
  // CopyOrMoveHookDelegate.
  std::vector<base::File::Error> status;
};

class CopyOrMoveHookDelegateCompositeTest
    : public testing::TestWithParam<TestParameter> {
 public:
  using MethodToTestCall =
      base::RepeatingCallback<void(CopyOrMoveHookDelegate*,
                                   const FileSystemURL&,
                                   const FileSystemURL&,
                                   CopyOrMoveHookDelegate::StatusCallback)>;
  using SimpleMethodToTestCall =
      base::RepeatingCallback<void(CopyOrMoveHookDelegate*)>;
  using MockMethodCall = base::RepeatingCallback<void(
      MockCopyOrMoveHookDelegate*,
      ::testing::Action<void(const FileSystemURL&,
                             const FileSystemURL&,
                             CopyOrMoveHookDelegate::StatusCallback)>)>;
  using SimpleMockMethodCall =
      base::RepeatingCallback<void(MockCopyOrMoveHookDelegate*)>;

  void SetStatus(base::File::Error status) { status_ = status; }

  std::unique_ptr<CopyOrMoveHookDelegate> BuildComposite(
      TestParameter param,
      MockMethodCall mock_method);

  std::unique_ptr<CopyOrMoveHookDelegate> BuildComposite(
      TestParameter param,
      SimpleMockMethodCall mock_method);

  // `to_test_method` is expected to call a method of CopyOrMoveHookDelegate on
  // the given instance with the given parameters. `mock_method` is called to
  // mock the correct method. This method is expected to be called once (per
  // instance). This should be checked by the mock and it should execute the
  // given action.
  void TestContinuation(MethodToTestCall to_test_method,
                        MockMethodCall mock_method);

  // `to_test_method` is expected to call a method of CopyOrMoveHookDelegate on
  // the given instance with the given parameter. `mock_method` is called to
  // mock the correct method. This method is expected to be called once (per
  // instance). This should be checked by the mock.
  void TestSimpleMethod(SimpleMethodToTestCall to_test_method,
                        SimpleMockMethodCall mock_method);

  FileSystemURL source_url_ =
      FileSystemURL::CreateForTest(GURL("filesystem:http://source"));
  FileSystemURL destination_url_ =
      FileSystemURL::CreateForTest(GURL("filesystem:http://destination"));

 private:
  friend class CopyOrMoveHookDelegateTestImpl;

  // Value of call to the StatusCallback.
  base::File::Error status_ = base::File::FILE_ERROR_ABORT;
};

std::unique_ptr<CopyOrMoveHookDelegate>
CopyOrMoveHookDelegateCompositeTest::BuildComposite(
    TestParameter param,
    MockMethodCall mock_method) {
  std::unique_ptr<CopyOrMoveHookDelegate> root =
      std::make_unique<CopyOrMoveHookDelegateComposite>();

  for (base::File::Error status : param.status) {
    std::unique_ptr<MockCopyOrMoveHookDelegate> hook =
        std::make_unique<::testing::StrictMock<MockCopyOrMoveHookDelegate>>();
    mock_method.Run(
        hook.get(),
        [status](const FileSystemURL& source, const FileSystemURL& destination,
                 CopyOrMoveHookDelegate::StatusCallback callback) {
          std::move(callback).Run(status);
        });
    root = CopyOrMoveHookDelegateComposite::CreateOrAdd(std::move(root),
                                                        std::move(hook));
  }

  return root;
}

std::unique_ptr<CopyOrMoveHookDelegate>
CopyOrMoveHookDelegateCompositeTest::BuildComposite(
    TestParameter param,
    SimpleMockMethodCall mock_method) {
  std::unique_ptr<CopyOrMoveHookDelegate> root =
      std::make_unique<CopyOrMoveHookDelegateComposite>();

  for (size_t i = 0; i < param.status.size(); ++i) {
    std::unique_ptr<MockCopyOrMoveHookDelegate> hook =
        std::make_unique<::testing::StrictMock<MockCopyOrMoveHookDelegate>>();
    mock_method.Run(hook.get());
    root = CopyOrMoveHookDelegateComposite::CreateOrAdd(std::move(root),
                                                        std::move(hook));
  }
  return root;
}

// This test takes a `delegate`, possibly a composite and calls the
// `to_test_method`. It is assumed that all CopyOrMoveHookDelegate objects are
// of type MockCopyOrMoveDelegate and will have the fitting method mocked
// and checked for exactly one execution. If the objects should fail on the
// method call, they do it with a base::File::Error code given by the test
// parameter.
void CopyOrMoveHookDelegateCompositeTest::TestContinuation(
    MethodToTestCall to_test_method,
    MockMethodCall mock_method) {
  TestParameter param = GetParam();

  std::unique_ptr<CopyOrMoveHookDelegate> hook =
      BuildComposite(param, mock_method);
  to_test_method.Run(
      hook.get(), source_url_, destination_url_,
      base::BindOnce(&CopyOrMoveHookDelegateCompositeTest::SetStatus,
                     base::Unretained(this)));

  // All instances end up in the same list.
  EXPECT_EQ(param.status.size(),
            static_cast<CopyOrMoveHookDelegateComposite*>(hook.get())
                ->delegates_.size());
  // Correct call on status callback.
  if (GetLeadingOK(param.status) >= param.status.size()) {
    EXPECT_EQ(base::File::FILE_OK, status_);
  } else {
    // The first non FILE_OK value is the value to be returned
    EXPECT_EQ(param.status[GetLeadingOK(param.status)], status_);
  }
}

// This test takes a `delegate`, possibly a composite and calls the
// `to_test_method`. It is assumed that all CopyOrMoveHookDelegate objects are
// of type MockCopyOrMoveHookDelegate and will have the fitting method mocked
// and checked for exactly one execution.
void CopyOrMoveHookDelegateCompositeTest::TestSimpleMethod(
    SimpleMethodToTestCall to_test_method,
    SimpleMockMethodCall mock_method) {
  TestParameter param = GetParam();
  std::unique_ptr<CopyOrMoveHookDelegate> hook =
      BuildComposite(param, mock_method);
  to_test_method.Run(hook.get());
  // All instances end up in the same list.
  EXPECT_EQ(param.status.size(),
            static_cast<CopyOrMoveHookDelegateComposite*>(hook.get())
                ->delegates_.size());
}

TEST_P(CopyOrMoveHookDelegateCompositeTest, OnBeginProcessFileP) {
  TestContinuation(
      base::BindRepeating(&CopyOrMoveHookDelegate::OnBeginProcessFile),
      base::BindRepeating(
          [](MockCopyOrMoveHookDelegate* hook,
             ::testing::Action<void(const FileSystemURL&, const FileSystemURL&,
                                    CopyOrMoveHookDelegate::StatusCallback)>
                 action) {
            EXPECT_CALL(*hook, OnBeginProcessFile).WillOnce(action);
          }));
}

TEST_P(CopyOrMoveHookDelegateCompositeTest, OnBeginProcessDirectoryP) {
  TestContinuation(
      base::BindRepeating(&CopyOrMoveHookDelegate::OnBeginProcessDirectory),
      base::BindRepeating(
          [](MockCopyOrMoveHookDelegate* hook,
             ::testing::Action<void(const FileSystemURL&, const FileSystemURL&,
                                    CopyOrMoveHookDelegate::StatusCallback)>
                 action) {
            EXPECT_CALL(*hook, OnBeginProcessDirectory).WillOnce(action);
          }));
}

TEST_P(CopyOrMoveHookDelegateCompositeTest, OnProgressP) {
  TestSimpleMethod(
      base::BindLambdaForTesting([&](CopyOrMoveHookDelegate* delegate) {
        delegate->OnProgress(source_url_, destination_url_, 0);
      }),
      base::BindRepeating([](MockCopyOrMoveHookDelegate* hook) {
        EXPECT_CALL(*hook, OnProgress).Times(1);
      }));
}

TEST_P(CopyOrMoveHookDelegateCompositeTest, OnErrorP) {
  TestSimpleMethod(
      base::BindLambdaForTesting([&](CopyOrMoveHookDelegate* delegate) {
        delegate->OnError(source_url_, destination_url_,
                          base::File::FILE_ERROR_FAILED, base::DoNothing());
      }),
      base::BindRepeating([](MockCopyOrMoveHookDelegate* hook) {
        EXPECT_CALL(*hook, OnError).Times(1);
      }));
}

TEST_P(CopyOrMoveHookDelegateCompositeTest, OnEndCopyP) {
  TestSimpleMethod(
      base::BindLambdaForTesting([&](CopyOrMoveHookDelegate* delegate) {
        delegate->OnEndCopy(source_url_, destination_url_);
      }),
      base::BindRepeating([](MockCopyOrMoveHookDelegate* hook) {
        EXPECT_CALL(*hook, OnEndCopy).Times(1);
      }));
}

TEST_P(CopyOrMoveHookDelegateCompositeTest, OnEndMoveP) {
  TestSimpleMethod(
      base::BindLambdaForTesting([&](CopyOrMoveHookDelegate* delegate) {
        delegate->OnEndMove(source_url_, destination_url_);
      }),
      base::BindRepeating([](MockCopyOrMoveHookDelegate* hook) {
        EXPECT_CALL(*hook, OnEndMove).Times(1);
      }));
}

TEST_P(CopyOrMoveHookDelegateCompositeTest, OnEndRemoveSourceP) {
  TestSimpleMethod(
      base::BindLambdaForTesting([&](CopyOrMoveHookDelegate* delegate) {
        delegate->OnEndRemoveSource(source_url_);
      }),
      base::BindRepeating([](MockCopyOrMoveHookDelegate* hook) {
        EXPECT_CALL(*hook, OnEndRemoveSource).Times(1);
      }));
}

INSTANTIATE_TEST_SUITE_P(
    storage,
    CopyOrMoveHookDelegateCompositeTest,
    testing::ValuesIn<TestParameter>({
        // Return status of the instances to be used in the test.
        {{base::File::FILE_OK}},
        {{base::File::FILE_OK, base::File::FILE_OK}},
        {{base::File::FILE_OK, base::File::FILE_OK, base::File::FILE_OK}},
        {{base::File::FILE_ERROR_ABORT}},
        {{base::File::FILE_ERROR_ABORT, base::File::FILE_OK}},
        {{base::File::FILE_ERROR_ABORT, base::File::FILE_ERROR_FAILED}},
        {{base::File::FILE_ERROR_ABORT, base::File::FILE_OK,
          base::File::FILE_ERROR_FAILED}},
        {{base::File::FILE_OK, base::File::FILE_ERROR_ABORT}},
        {{base::File::FILE_OK, base::File::FILE_ERROR_ABORT,
          base::File::FILE_OK}},
        {{base::File::FILE_OK, base::File::FILE_ERROR_ABORT,
          base::File::FILE_ERROR_FAILED}},
        {{base::File::FILE_OK, base::File::FILE_ERROR_ABORT,
          base::File::FILE_OK, base::File::FILE_ERROR_FAILED}},
    }));

}  // namespace storage
