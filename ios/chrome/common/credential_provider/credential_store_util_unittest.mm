// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/credential_store_util.h"

#import "base/memory/weak_ptr.h"
#import "base/task/bind_post_task.h"
#import "base/task/sequenced_task_runner.h"
#import "base/test/ios/wait_util.h"
#import "base/test/task_environment.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"
#import "ios/chrome/common/credential_provider/memory_credential_store.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

class CredentialStoreUtilTest : public PlatformTest {
 public:
  base::test::SingleThreadTaskEnvironment env_;
};

// Returns a new credential with dummy values. Specifically, username is set to
// @"userN" and password is set to @"passwordN", where "N" is the contents of
// `index`.

ArchivableCredential* GetTestCredential(NSString* index) {
  return [[ArchivableCredential alloc]
               initWithFavicon:@"favicon"
                          gaia:nil
                      password:[@"password" stringByAppendingString:index]
                          rank:5
              recordIdentifier:[@"recordIdentifier"
                                   stringByAppendingString:index]
             serviceIdentifier:@"serviceIdentifier"
                   serviceName:@"serviceName"
      registryControlledDomain:@"example.com"
                      username:[@"user" stringByAppendingString:index]
                          note:@"note"];
}

// Returns true iff `container` contains a credential with username @"userN"
// where "N" is the contents of `index`.
bool ContainsTestCredential(NSArray<id<Credential>>* container,
                            NSString* index) {
  return [container indexOfObjectPassingTest:^(id<Credential> cred,
                                               NSUInteger unused, BOOL* stop) {
           return [cred.username
               isEqualToString:[@"user" stringByAppendingString:index]];
         }] != NSNotFound;
}

}  // namespace

TEST_F(CredentialStoreUtilTest, ReadFromMultipleCredentialStoresAsync) {
  // Use real MemoryCredentialStore rather than Mock so that we actually
  // dispatch tasks to queues.
  MemoryCredentialStore* credential_store_1 =
      [[MemoryCredentialStore alloc] init];
  [credential_store_1 addCredential:GetTestCredential(@"1")];
  [credential_store_1 addCredential:GetTestCredential(@"2")];

  MemoryCredentialStore* credential_store_2 =
      [[MemoryCredentialStore alloc] init];
  [credential_store_2 addCredential:GetTestCredential(@"3")];
  [credential_store_2 addCredential:GetTestCredential(@"4")];

  // Wrap Obj-C pointer in a struct to allow a C++ callback to write to it
  // without taking ownership.
  struct CredentialArrayHolder {
    NSArray<id<Credential>>* array = nil;
    base::WeakPtrFactory<CredentialArrayHolder> weak_factory{this};
  } all_credentials;

  auto cb = base::BindOnce(
      [](base::WeakPtr<CredentialArrayHolder> holder,
         NSArray<id<Credential>>* result) {
        // If a task goes rogue and outlives the holder, fail and halt rather
        // than writing to invalid memory and potentially corrupting subsequent
        // tests.
        ASSERT_TRUE(holder);
        holder->array = result;
      },
      all_credentials.weak_factory.GetWeakPtr());

  credential_store_util::ReadFromMultipleCredentialStoresAsync(
      @[ credential_store_1, credential_store_2 ],
      // Post to a runner on this thread to make the WeakPtr usage safe.
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         std::move(cb)));

  base::WeakPtr<CredentialArrayHolder> weak_holder =
      all_credentials.weak_factory.GetWeakPtr();
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout,
      /* run_message_loop = */ true, ^BOOL {
        // We have to CHECK rather than ASSERT because ASSERT adds an implicit
        // return with type void. Crashing is still better than writing to
        // deallocated memory, and in any case it should never happen.
        CHECK(weak_holder);
        return weak_holder->array.count == 4;
      }));

  for (NSString* index in @[ @"1", @"2", @"3", @"4" ]) {
    EXPECT_TRUE(ContainsTestCredential(all_credentials.array, index))
        << @"Could not find credential with index `" << index
        << @"` in container.";
  }
}
