// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/session_restoration_web_state_list_observer.h"

#import "base/containers/contains.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"

namespace {

// Increments count when called.
void IncrementCounter(size_t* counter) {
  ++*counter;
}

// State in which the WebState is created.
enum class CreateWebStateAs {
  kUnrealized,
  kSerializable,
  kUnserializable,
};

}  // anonymous namespace

class SessionRestorationWebStateListObserverTest : public PlatformTest {
 public:
  WebStateList* web_state_list() { return &web_state_list_; }

  std::unique_ptr<web::FakeWebState> CreateWebState(CreateWebStateAs state) {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetWebFramesManager(
        std::make_unique<web::FakeWebFramesManager>());
    web_state->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());

    // Initialize the state of the WebState.
    switch (state) {
      case CreateWebStateAs::kUnrealized:
        web_state->SetIsRealized(false);
        break;

      case CreateWebStateAs::kSerializable:
        web_state->SetIsRealized(true);
        break;

      case CreateWebStateAs::kUnserializable:
        web::FakeNavigationManager* manager =
            static_cast<web::FakeNavigationManager*>(
                web_state->GetNavigationManager());
        manager->AddItem(GURL("https://www.example.com"),
                         ui::PAGE_TRANSITION_LINK);
        manager->SetPendingItem(manager->GetLastCommittedItem());
        manager->SetLastCommittedItem(nullptr);
        break;
    }

    return web_state;
  }

  web::FakeWebState* InsertWebState(
      std::unique_ptr<web::FakeWebState> web_state) {
    const int insertion_index =
        web_state_list_.InsertWebState(std::move(web_state));
    return static_cast<web::FakeWebState*>(
        web_state_list_.GetWebStateAt(insertion_index));
  }

 private:
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_{&web_state_list_delegate_};
};

// Tests that SessionRestorationWebStateListObserver consider the WebStateList
// as clean on creation.
TEST_F(SessionRestorationWebStateListObserverTest, Creation) {
  size_t call_count = 0;
  SessionRestorationWebStateListObserver observer(
      web_state_list(), base::IgnoreArgs<WebStateList*>(base::BindRepeating(
                            &IncrementCounter, &call_count)));

  EXPECT_FALSE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
  EXPECT_EQ(call_count, 0u);
}

// Tests that SessionRestorationWebStateListObserver consider the WebStateList
// and the inserted WebState as dirty when inserting a WebState that can be
// serialized.
TEST_F(SessionRestorationWebStateListObserverTest, Insert) {
  size_t call_count = 0;
  SessionRestorationWebStateListObserver observer(
      web_state_list(), base::IgnoreArgs<WebStateList*>(base::BindRepeating(
                            &IncrementCounter, &call_count)));

  web::WebState* const web_state =
      InsertWebState(CreateWebState(CreateWebStateAs::kSerializable));

  EXPECT_TRUE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(base::Contains(observer.dirty_web_states(), web_state));
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
  EXPECT_EQ(call_count, 1u);

  // Check that calling ClearDirty() leaves the observer in a non-dirty state.
  observer.ClearDirty();

  EXPECT_FALSE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
}

// Tests that SessionRestorationWebStateListObserver consider the WebStateList
// as dirty when inserting an unrealized WebState.
TEST_F(SessionRestorationWebStateListObserverTest, Insert_Unrealized) {
  size_t call_count = 0;
  SessionRestorationWebStateListObserver observer(
      web_state_list(), base::IgnoreArgs<WebStateList*>(base::BindRepeating(
                            &IncrementCounter, &call_count)));

  web::WebState* const web_state =
      InsertWebState(CreateWebState(CreateWebStateAs::kUnrealized));
  const web::WebStateID web_state_id = web_state->GetUniqueIdentifier();

  EXPECT_TRUE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(base::Contains(observer.inserted_web_states(), web_state_id));
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
  EXPECT_EQ(call_count, 1u);

  // Check that calling ClearDirty() leaves the observer in a non-dirty state.
  observer.ClearDirty();

  EXPECT_FALSE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
}

// Tests that SessionRestorationWebStateListObserver consider the WebStateList
// as dirty when inserting a WebState whose navigation history is still being
// restored.
TEST_F(SessionRestorationWebStateListObserverTest, Insert_Unserializable) {
  size_t call_count = 0;
  SessionRestorationWebStateListObserver observer(
      web_state_list(), base::IgnoreArgs<WebStateList*>(base::BindRepeating(
                            &IncrementCounter, &call_count)));

  web::WebState* const web_state =
      InsertWebState(CreateWebState(CreateWebStateAs::kUnserializable));

  EXPECT_TRUE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(base::Contains(observer.dirty_web_states(), web_state));
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
  EXPECT_EQ(call_count, 1u);

  // Check that calling ClearDirty() leaves the observer in a non-dirty state.
  observer.ClearDirty();

  EXPECT_FALSE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
}

// Tests that SessionRestorationWebStateListObserver consider the WebStateList
// and the inserted WebStates as dirty when inserting multiple WebStates that
// can be serialized.
TEST_F(SessionRestorationWebStateListObserverTest, Insert_MultipleWebStates) {
  size_t call_count = 0;
  SessionRestorationWebStateListObserver observer(
      web_state_list(), base::IgnoreArgs<WebStateList*>(base::BindRepeating(
                            &IncrementCounter, &call_count)));

  web::WebState* const web_state_1 =
      InsertWebState(CreateWebState(CreateWebStateAs::kSerializable));
  web::WebState* const web_state_2 =
      InsertWebState(CreateWebState(CreateWebStateAs::kSerializable));

  EXPECT_TRUE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(base::Contains(observer.dirty_web_states(), web_state_1));
  EXPECT_TRUE(base::Contains(observer.dirty_web_states(), web_state_2));
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
  EXPECT_EQ(call_count, 1u);  // The callback is only called once!

  // Check that calling ClearDirty() leaves the observer in a non-dirty state.
  observer.ClearDirty();

  EXPECT_FALSE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
}

// Tests that SessionRestorationWebStateListObserver consider the WebStateList
// as clean after calling ClearDirty().
TEST_F(SessionRestorationWebStateListObserverTest, ClearDirty) {
  size_t call_count = 0;
  SessionRestorationWebStateListObserver observer(
      web_state_list(), base::IgnoreArgs<WebStateList*>(base::BindRepeating(
                            &IncrementCounter, &call_count)));

  // Insert a WebState to force the observer to be marked as dirty.
  web::WebState* const web_state =
      InsertWebState(CreateWebState(CreateWebStateAs::kSerializable));

  EXPECT_TRUE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(base::Contains(observer.dirty_web_states(), web_state));
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
  EXPECT_EQ(call_count, 1u);

  // Clear the dirty state.
  observer.ClearDirty();

  EXPECT_FALSE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
  EXPECT_EQ(call_count, 1u);  // The callback is not invoked by ClearDirty()!
}

// Tests that SessionRestorationWebStateListObserver consider the WebStateList
// as dirty when detaching a serializable WebState. The WebState is not listed
// as up for adoption as it is realized.
TEST_F(SessionRestorationWebStateListObserverTest, Detach) {
  size_t call_count = 0;
  SessionRestorationWebStateListObserver observer(
      web_state_list(), base::IgnoreArgs<WebStateList*>(base::BindRepeating(
                            &IncrementCounter, &call_count)));

  InsertWebState(CreateWebState(CreateWebStateAs::kSerializable));

  // Clear the dirty state and reset the call counter.
  observer.ClearDirty();
  call_count = 0;

  ASSERT_GT(web_state_list()->count(), 0);
  web_state_list()->DetachWebStateAt(/*index*/ 0);

  EXPECT_TRUE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
  EXPECT_EQ(call_count, 1u);

  // Check that calling ClearDirty() leaves the observer in a non-dirty state.
  observer.ClearDirty();

  EXPECT_FALSE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
}

// Tests that SessionRestorationWebStateListObserver consider the WebStateList
// as dirty when detaching an unrealized WebState. The WebState is listed as up
// for adoption if unrealized.
TEST_F(SessionRestorationWebStateListObserverTest, Detach_Unrealized) {
  size_t call_count = 0;
  SessionRestorationWebStateListObserver observer(
      web_state_list(), base::IgnoreArgs<WebStateList*>(base::BindRepeating(
                            &IncrementCounter, &call_count)));

  web::WebState* const web_state =
      InsertWebState(CreateWebState(CreateWebStateAs::kUnrealized));
  const web::WebStateID web_state_id = web_state->GetUniqueIdentifier();

  // Clear the dirty state and reset the call counter.
  observer.ClearDirty();
  call_count = 0;

  ASSERT_GT(web_state_list()->count(), 0);
  web_state_list()->DetachWebStateAt(/*index*/ 0);

  EXPECT_TRUE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(base::Contains(observer.detached_web_states(), web_state_id));
  EXPECT_TRUE(observer.closed_web_states().empty());
  EXPECT_EQ(call_count, 1u);

  // Check that calling ClearDirty() leaves the observer in a non-dirty state.
  observer.ClearDirty();

  EXPECT_FALSE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
}

// Tests that SessionRestorationWebStateListObserver consider the WebStateList
// as dirty when detaching a WebState whose navigation history is still being
// restored. The WebState is not listed as up for adoption as it is realized.
TEST_F(SessionRestorationWebStateListObserverTest, Detach_Unserializable) {
  size_t call_count = 0;
  SessionRestorationWebStateListObserver observer(
      web_state_list(), base::IgnoreArgs<WebStateList*>(base::BindRepeating(
                            &IncrementCounter, &call_count)));

  InsertWebState(CreateWebState(CreateWebStateAs::kUnserializable));

  // Clear the dirty state and reset the call counter.
  observer.ClearDirty();
  call_count = 0;

  ASSERT_GT(web_state_list()->count(), 0);
  web_state_list()->DetachWebStateAt(/*index*/ 0);

  EXPECT_TRUE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
  EXPECT_EQ(call_count, 1u);

  // Check that calling ClearDirty() leaves the observer in a non-dirty state.
  observer.ClearDirty();

  EXPECT_FALSE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
}

// Tests that SessionRestorationWebStateListObserver consider the WebStateList
// as dirty when detaching a serializable WebState that has just been inserted.
// The WebState is not listed as up for adoption, and is no longer listed as
// dirty.
TEST_F(SessionRestorationWebStateListObserverTest, Detach_Dirty) {
  size_t call_count = 0;
  SessionRestorationWebStateListObserver observer(
      web_state_list(), base::IgnoreArgs<WebStateList*>(base::BindRepeating(
                            &IncrementCounter, &call_count)));

  InsertWebState(CreateWebState(CreateWebStateAs::kSerializable));

  ASSERT_GT(web_state_list()->count(), 0);
  web_state_list()->DetachWebStateAt(/*index*/ 0);

  EXPECT_TRUE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
  EXPECT_EQ(call_count, 1u);  // The callback is only called once!

  // Check that calling ClearDirty() leaves the observer in a non-dirty state.
  observer.ClearDirty();

  EXPECT_FALSE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
}

// Tests that SessionRestorationWebStateListObserver consider the WebStateList
// as dirty when detaching an unrealized WebState that has just been inserted.
// The WebState is no longer listed as inserted and is not listed for adoption
// either.
TEST_F(SessionRestorationWebStateListObserverTest, Detach_DirtyUnrealized) {
  size_t call_count = 0;
  SessionRestorationWebStateListObserver observer(
      web_state_list(), base::IgnoreArgs<WebStateList*>(base::BindRepeating(
                            &IncrementCounter, &call_count)));

  InsertWebState(CreateWebState(CreateWebStateAs::kUnrealized));

  ASSERT_GT(web_state_list()->count(), 0);
  web_state_list()->DetachWebStateAt(/*index*/ 0);

  EXPECT_TRUE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
  EXPECT_EQ(call_count, 1u);  // The callback is only called once!

  // Check that calling ClearDirty() leaves the observer in a non-dirty state.
  observer.ClearDirty();

  EXPECT_FALSE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
}

// Tests that SessionRestorationWebStateListObserver consider the WebStateList
// as dirty when detaching a WebState  that has just been inserted whose
// navigation history is still being restored. The WebState is no longer listed
// as inserted and is not listed for adoption either.
TEST_F(SessionRestorationWebStateListObserverTest, Detach_DirtyUnserializable) {
  size_t call_count = 0;
  SessionRestorationWebStateListObserver observer(
      web_state_list(), base::IgnoreArgs<WebStateList*>(base::BindRepeating(
                            &IncrementCounter, &call_count)));

  InsertWebState(CreateWebState(CreateWebStateAs::kUnserializable));

  ASSERT_GT(web_state_list()->count(), 0);
  web_state_list()->DetachWebStateAt(/*index*/ 0);

  EXPECT_TRUE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
  EXPECT_EQ(call_count, 1u);  // The callback is only called once!

  // Check that calling ClearDirty() leaves the observer in a non-dirty state.
  observer.ClearDirty();

  EXPECT_FALSE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
}

// Tests that SessionRestorationWebStateListObserver consider the WebStateList
// as dirty when closing a serializable WebState. The WebState is not listed as
// up for adoption.
TEST_F(SessionRestorationWebStateListObserverTest, Close) {
  size_t call_count = 0;
  SessionRestorationWebStateListObserver observer(
      web_state_list(), base::IgnoreArgs<WebStateList*>(base::BindRepeating(
                            &IncrementCounter, &call_count)));

  web::WebState* const web_state =
      InsertWebState(CreateWebState(CreateWebStateAs::kSerializable));
  const web::WebStateID web_state_id = web_state->GetUniqueIdentifier();

  // Clear the dirty state and reset the call counter.
  observer.ClearDirty();
  call_count = 0;

  ASSERT_GT(web_state_list()->count(), 0);
  web_state_list()->CloseWebStateAt(/*index*/ 0, WebStateList::CLOSE_NO_FLAGS);

  EXPECT_TRUE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(base::Contains(observer.closed_web_states(), web_state_id));
  EXPECT_EQ(call_count, 1u);

  // Check that calling ClearDirty() leaves the observer in a non-dirty state.
  observer.ClearDirty();

  EXPECT_FALSE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
}

// Tests that SessionRestorationWebStateListObserver consider the WebStateList
// as dirty when closing an unrealized WebState. The WebState is not listed as
// up for adoption.
TEST_F(SessionRestorationWebStateListObserverTest, Close_Unrealized) {
  size_t call_count = 0;
  SessionRestorationWebStateListObserver observer(
      web_state_list(), base::IgnoreArgs<WebStateList*>(base::BindRepeating(
                            &IncrementCounter, &call_count)));

  web::WebState* const web_state =
      InsertWebState(CreateWebState(CreateWebStateAs::kUnrealized));
  const web::WebStateID web_state_id = web_state->GetUniqueIdentifier();

  // Clear the dirty state and reset the call counter.
  observer.ClearDirty();
  call_count = 0;

  ASSERT_GT(web_state_list()->count(), 0);
  web_state_list()->CloseWebStateAt(/*index*/ 0, WebStateList::CLOSE_NO_FLAGS);

  EXPECT_TRUE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(base::Contains(observer.closed_web_states(), web_state_id));
  EXPECT_EQ(call_count, 1u);

  // Check that calling ClearDirty() leaves the observer in a non-dirty state.
  observer.ClearDirty();

  EXPECT_FALSE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
}

// Tests that SessionRestorationWebStateListObserver consider the WebStateList
// as dirty when closing a WebState whose navigation history is still being
// restored. The WebState is listed as up for adoption.
TEST_F(SessionRestorationWebStateListObserverTest, Close_Unserializable) {
  size_t call_count = 0;
  SessionRestorationWebStateListObserver observer(
      web_state_list(), base::IgnoreArgs<WebStateList*>(base::BindRepeating(
                            &IncrementCounter, &call_count)));

  web::WebState* const web_state =
      InsertWebState(CreateWebState(CreateWebStateAs::kUnserializable));
  const web::WebStateID web_state_id = web_state->GetUniqueIdentifier();

  // Clear the dirty state and reset the call counter.
  observer.ClearDirty();
  call_count = 0;

  ASSERT_GT(web_state_list()->count(), 0);
  web_state_list()->CloseWebStateAt(/*index*/ 0, WebStateList::CLOSE_NO_FLAGS);

  EXPECT_TRUE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(base::Contains(observer.closed_web_states(), web_state_id));
  EXPECT_EQ(call_count, 1u);

  // Check that calling ClearDirty() leaves the observer in a non-dirty state.
  observer.ClearDirty();

  EXPECT_FALSE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
}

// Tests that SessionRestorationWebStateListObserver consider the WebStateList
// as dirty when closing a serializable WebState that has just been inserted.
// The WebState is not listed as up for adoption, and is no longer listed as
// dirty.
TEST_F(SessionRestorationWebStateListObserverTest, Close_Dirty) {
  size_t call_count = 0;
  SessionRestorationWebStateListObserver observer(
      web_state_list(), base::IgnoreArgs<WebStateList*>(base::BindRepeating(
                            &IncrementCounter, &call_count)));

  web::WebState* const web_state =
      InsertWebState(CreateWebState(CreateWebStateAs::kSerializable));
  const web::WebStateID web_state_id = web_state->GetUniqueIdentifier();

  ASSERT_GT(web_state_list()->count(), 0);
  web_state_list()->CloseWebStateAt(/*index*/ 0, WebStateList::CLOSE_NO_FLAGS);

  EXPECT_TRUE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(base::Contains(observer.closed_web_states(), web_state_id));
  EXPECT_EQ(call_count, 1u);  // The callback is only called once!

  // Check that calling ClearDirty() leaves the observer in a non-dirty state.
  observer.ClearDirty();

  EXPECT_FALSE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
}

// Tests that SessionRestorationWebStateListObserver consider the WebStateList
// as dirty when closing an unrealized WebState that has just been inserted. The
// WebState is no longer listed as inserted and is not listed for adoption
// either.
TEST_F(SessionRestorationWebStateListObserverTest, Close_DirtyUnrealized) {
  size_t call_count = 0;
  SessionRestorationWebStateListObserver observer(
      web_state_list(), base::IgnoreArgs<WebStateList*>(base::BindRepeating(
                            &IncrementCounter, &call_count)));

  web::WebState* const web_state =
      InsertWebState(CreateWebState(CreateWebStateAs::kUnrealized));
  const web::WebStateID web_state_id = web_state->GetUniqueIdentifier();

  ASSERT_GT(web_state_list()->count(), 0);
  web_state_list()->CloseWebStateAt(/*index*/ 0, WebStateList::CLOSE_NO_FLAGS);

  EXPECT_TRUE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(base::Contains(observer.closed_web_states(), web_state_id));
  EXPECT_EQ(call_count, 1u);  // The callback is only called once!

  // Check that calling ClearDirty() leaves the observer in a non-dirty state.
  observer.ClearDirty();

  EXPECT_FALSE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
}

// Tests that SessionRestorationWebStateListObserver consider the WebStateList
// as dirty when closing a WebState that has just been inserted whose navigation
// history is still being restored. The WebState is no longer listed as inserted
// and is not listed for adoption either.
TEST_F(SessionRestorationWebStateListObserverTest, Close_DirtyUnserializable) {
  size_t call_count = 0;
  SessionRestorationWebStateListObserver observer(
      web_state_list(), base::IgnoreArgs<WebStateList*>(base::BindRepeating(
                            &IncrementCounter, &call_count)));

  web::WebState* const web_state =
      InsertWebState(CreateWebState(CreateWebStateAs::kUnserializable));
  const web::WebStateID web_state_id = web_state->GetUniqueIdentifier();

  ASSERT_GT(web_state_list()->count(), 0);
  web_state_list()->CloseWebStateAt(/*index*/ 0, WebStateList::CLOSE_NO_FLAGS);

  EXPECT_TRUE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(base::Contains(observer.closed_web_states(), web_state_id));
  EXPECT_EQ(call_count, 1u);  // The callback is only called once!

  // Check that calling ClearDirty() leaves the observer in a non-dirty state.
  observer.ClearDirty();

  EXPECT_FALSE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
}

// Tests that SessionRestorationWebStateListObserver consider the WebStateList
// as dirty when moving WebState, but no WebStates are considered dirty.
TEST_F(SessionRestorationWebStateListObserverTest, Move) {
  size_t call_count = 0;
  SessionRestorationWebStateListObserver observer(
      web_state_list(), base::IgnoreArgs<WebStateList*>(base::BindRepeating(
                            &IncrementCounter, &call_count)));

  InsertWebState(CreateWebState(CreateWebStateAs::kSerializable));
  InsertWebState(CreateWebState(CreateWebStateAs::kSerializable));

  // Clear the dirty state and reset the call counter.
  observer.ClearDirty();
  call_count = 0;

  ASSERT_GE(web_state_list()->count(), 2);
  web_state_list()->MoveWebStateAt(/*from_index*/ 0, /*to_index*/ 1);

  EXPECT_TRUE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
  EXPECT_EQ(call_count, 1u);

  // Check that calling ClearDirty() leaves the observer in a non-dirty state.
  observer.ClearDirty();

  EXPECT_FALSE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
}

// Tests that SessionRestorationWebStateListObserver consider the WebStateList
// as dirty when changing the active WebState, but no WebStates are considered
// dirty.
TEST_F(SessionRestorationWebStateListObserverTest, Activate) {
  size_t call_count = 0;
  SessionRestorationWebStateListObserver observer(
      web_state_list(), base::IgnoreArgs<WebStateList*>(base::BindRepeating(
                            &IncrementCounter, &call_count)));

  InsertWebState(CreateWebState(CreateWebStateAs::kSerializable));
  InsertWebState(CreateWebState(CreateWebStateAs::kSerializable));

  // Clear the dirty state and reset the call counter.
  observer.ClearDirty();
  call_count = 0;

  ASSERT_GE(web_state_list()->count(), 2);
  ASSERT_EQ(web_state_list()->active_index(), WebStateList::kInvalidIndex);
  web_state_list()->ActivateWebStateAt(/*index*/ 0);

  EXPECT_TRUE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
  EXPECT_EQ(call_count, 1u);

  // Check that calling ClearDirty() leaves the observer in a non-dirty state.
  observer.ClearDirty();

  EXPECT_FALSE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
}

// Tests that SessionRestorationWebStateListObserver consider the WebStateList
// and the inserted WebState as dirty when replacing a WebState.
TEST_F(SessionRestorationWebStateListObserverTest, Replace) {
  size_t call_count = 0;
  SessionRestorationWebStateListObserver observer(
      web_state_list(), base::IgnoreArgs<WebStateList*>(base::BindRepeating(
                            &IncrementCounter, &call_count)));

  web::WebState* const web_state =
      InsertWebState(CreateWebState(CreateWebStateAs::kSerializable));
  const web::WebStateID web_state_id = web_state->GetUniqueIdentifier();

  // Clear the dirty state and reset the call counter.
  observer.ClearDirty();
  call_count = 0;

  ASSERT_GT(web_state_list()->count(), 0);
  web_state_list()->ReplaceWebStateAt(
      /*index*/ 0, CreateWebState(CreateWebStateAs::kSerializable));
  web::WebState* const new_web_state =
      web_state_list()->GetWebStateAt(/*index*/ 0);

  EXPECT_TRUE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(base::Contains(observer.dirty_web_states(), new_web_state));
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(base::Contains(observer.closed_web_states(), web_state_id));
  EXPECT_EQ(call_count, 1u);

  // Check that calling ClearDirty() leaves the observer in a non-dirty state.
  observer.ClearDirty();

  EXPECT_FALSE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
}

// Tests that SessionRestorationWebStateListObserver consider the WebStateList
// as dirty after a batch operation (even if no change occurred).
TEST_F(SessionRestorationWebStateListObserverTest, BatchOperation) {
  size_t call_count = 0;
  SessionRestorationWebStateListObserver observer(
      web_state_list(), base::IgnoreArgs<WebStateList*>(base::BindRepeating(
                            &IncrementCounter, &call_count)));

  // Perform a batch operation to does nothing. The WebStateList should
  // be considered as dirty anyway.
  {
    WebStateList::ScopedBatchOperation lock =
        web_state_list()->StartBatchOperation();
  }

  EXPECT_TRUE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
  EXPECT_EQ(call_count, 1u);

  // Check that calling ClearDirty() leaves the observer in a non-dirty state.
  observer.ClearDirty();

  EXPECT_FALSE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
}

// Tests that SessionRestorationWebStateListObserver calls the callback when a
// WebState becomes dirty, even if the WebStateList itself has not changed.
TEST_F(SessionRestorationWebStateListObserverTest, WebStateDirty) {
  size_t call_count = 0;
  SessionRestorationWebStateListObserver observer(
      web_state_list(), base::IgnoreArgs<WebStateList*>(base::BindRepeating(
                            &IncrementCounter, &call_count)));

  web::FakeWebState* const web_state =
      InsertWebState(CreateWebState(CreateWebStateAs::kSerializable));

  // Clear the dirty state and reset the call counter.
  observer.ClearDirty();
  call_count = 0;

  web::FakeNavigationContext context;
  web_state->OnNavigationFinished(&context);

  EXPECT_FALSE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(base::Contains(observer.dirty_web_states(), web_state));
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
  EXPECT_EQ(call_count, 1u);

  // Check that calling ClearDirty() leaves the observer in a non-dirty state.
  observer.ClearDirty();

  EXPECT_FALSE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
}

// Tests that SessionRestorationWebStateListObserver call the callback
// when an WebState whose navigation restoration is still in progress becomes
// dirty.
TEST_F(SessionRestorationWebStateListObserverTest,
       WebStateDirty_Unserializable) {
  size_t call_count = 0;
  SessionRestorationWebStateListObserver observer(
      web_state_list(), base::IgnoreArgs<WebStateList*>(base::BindRepeating(
                            &IncrementCounter, &call_count)));

  web::FakeWebState* const web_state =
      InsertWebState(CreateWebState(CreateWebStateAs::kUnserializable));

  // Clear the dirty state and reset the call counter.
  observer.ClearDirty();
  call_count = 0;

  web::FakeNavigationContext context;
  web_state->OnNavigationFinished(&context);

  EXPECT_FALSE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(base::Contains(observer.dirty_web_states(), web_state));
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
  EXPECT_EQ(call_count, 1u);

  // Check that calling ClearDirty() leaves the observer in a non-dirty state.
  observer.ClearDirty();

  EXPECT_FALSE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
}

// Tests that SessionRestorationWebStateListObserver does not call the
// callback when an unrealized WebState becomes realized.
TEST_F(SessionRestorationWebStateListObserverTest, WebStateRealized) {
  size_t call_count = 0;
  SessionRestorationWebStateListObserver observer(
      web_state_list(), base::IgnoreArgs<WebStateList*>(base::BindRepeating(
                            &IncrementCounter, &call_count)));

  web::FakeWebState* const web_state =
      InsertWebState(CreateWebState(CreateWebStateAs::kUnrealized));

  // Clear the dirty state and reset the call counter.
  observer.ClearDirty();
  call_count = 0;

  web_state->ForceRealized();

  EXPECT_FALSE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
  EXPECT_EQ(call_count, 0u);  // Callback is not called!

  // Check that calling ClearDirty() leaves the observer in a non-dirty state.
  observer.ClearDirty();

  EXPECT_FALSE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.dirty_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
}

// Tests that if a WebState is marked as expected, it will not be added to the
// list of WebState to be adopted by SessionRestorationWebStateListObserver.
TEST_F(SessionRestorationWebStateListObserverTest, AddExpectedWebState) {
  size_t call_count = 0;
  SessionRestorationWebStateListObserver observer(
      web_state_list(), base::IgnoreArgs<WebStateList*>(base::BindRepeating(
                            &IncrementCounter, &call_count)));

  std::unique_ptr<web::FakeWebState> web_state =
      CreateWebState(CreateWebStateAs::kUnrealized);
  observer.AddExpectedWebState(web_state->GetUniqueIdentifier());
  InsertWebState(std::move(web_state));

  EXPECT_TRUE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
  EXPECT_EQ(call_count, 1u);
}

// Tests that if a WebState is detached and then reinserted to the same Browser
// the observer does not report it as either detached or inserted.
TEST_F(SessionRestorationWebStateListObserverTest, DetachAndInsertBack) {
  size_t call_count = 0;
  SessionRestorationWebStateListObserver observer(
      web_state_list(), base::IgnoreArgs<WebStateList*>(base::BindRepeating(
                            &IncrementCounter, &call_count)));

  std::unique_ptr<web::FakeWebState> web_state =
      CreateWebState(CreateWebStateAs::kUnrealized);
  const web::WebStateID web_state_id = web_state->GetUniqueIdentifier();
  observer.AddExpectedWebState(web_state_id);
  InsertWebState(std::move(web_state));

  EXPECT_TRUE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
  EXPECT_EQ(call_count, 1u);

  observer.ClearDirty();
  call_count = 0;

  // Detach the WebState, check that it is counted as detached.
  ASSERT_GE(web_state_list()->count(), 1);
  std::unique_ptr<web::WebState> detached_web_state =
      web_state_list()->DetachWebStateAt(0);
  ASSERT_EQ(detached_web_state->GetUniqueIdentifier(), web_state_id);

  EXPECT_TRUE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(base::Contains(observer.detached_web_states(), web_state_id));
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
  EXPECT_EQ(call_count, 1u);

  // Insert the WebState, check that it is no longer in the list of detached,
  // but was not added to the inserted WebState either.
  web_state_list()->InsertWebState(std::move(detached_web_state));

  EXPECT_TRUE(observer.is_web_state_list_dirty());
  EXPECT_TRUE(observer.detached_web_states().empty());
  EXPECT_TRUE(observer.inserted_web_states().empty());
  EXPECT_TRUE(observer.closed_web_states().empty());
  EXPECT_EQ(call_count, 1u);
}
