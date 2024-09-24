// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/test_session_restoration_service.h"

#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/session/proto/metadata.pb.h"
#import "ios/web/public/session/proto/storage.pb.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_id.h"

TestSessionRestorationService::TestSessionRestorationService() = default;

TestSessionRestorationService::~TestSessionRestorationService() = default;

// static
BrowserStateKeyedServiceFactory::TestingFactory
TestSessionRestorationService::GetTestingFactory() {
  return base::BindRepeating(
      [](web::BrowserState*) -> std::unique_ptr<KeyedService> {
        return std::make_unique<TestSessionRestorationService>();
      });
}

void TestSessionRestorationService::AddObserver(
    SessionRestorationObserver* observer) {
  observers_.AddObserver(observer);
}

void TestSessionRestorationService::RemoveObserver(
    SessionRestorationObserver* observer) {
  observers_.RemoveObserver(observer);
}

void TestSessionRestorationService::SaveSessions() {
  // Nothing to do.
}

void TestSessionRestorationService::ScheduleSaveSessions() {
  // Nothing to do.
}

void TestSessionRestorationService::SetSessionID(
    Browser* browser,
    const std::string& identifier) {
  // Nothing to do.
}

void TestSessionRestorationService::LoadWebStateStorage(
    Browser* browser,
    web::WebState* web_state,
    WebStateStorageCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), web::proto::WebStateStorage{}));
}

void TestSessionRestorationService::LoadSession(Browser* browser) {
  // Pretend loading will happen.
  for (SessionRestorationObserver& observer : observers_) {
    observer.WillStartSessionRestoration(browser);
  }

  // Pretend loading was successfull and an empty session was loaded.
  const std::vector<web::WebState*> restored_web_states;
  for (SessionRestorationObserver& observer : observers_) {
    observer.SessionRestorationFinished(browser, restored_web_states);
  }
}

void TestSessionRestorationService::AttachBackup(Browser* browser,
                                                 Browser* backup) {
  // Nothing to do.
}

void TestSessionRestorationService::Disconnect(Browser* browser) {
  // Nothing to do.
}

std::unique_ptr<web::WebState>
TestSessionRestorationService::CreateUnrealizedWebState(
    Browser* browser,
    web::proto::WebStateStorage storage) {
  // Extract the metadata from `storage`.
  web::proto::WebStateMetadataStorage metadata;
  metadata.Swap(storage.mutable_metadata());

  return web::WebState::CreateWithStorage(
      browser->GetProfile(), web::WebStateID::NewUnique(), std::move(metadata),
      base::ReturnValueOnce(std::move(storage)),
      base::ReturnValueOnce<NSData*>(nil));
}

void TestSessionRestorationService::DeleteDataForDiscardedSessions(
    const std::set<std::string>& identifiers,
    base::OnceClosure closure) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(closure));
}

void TestSessionRestorationService::InvokeClosureWhenBackgroundProcessingDone(
    base::OnceClosure closure) {
  std::move(closure).Run();
}

void TestSessionRestorationService::PurgeUnassociatedData(
    base::OnceClosure closure) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(closure));
}

void TestSessionRestorationService::ParseDataForBrowserAsync(
    Browser* browser,
    WebStateStorageIterationCallback iter_callback,
    WebStateStorageIterationCompleteCallback done) {
  // Pretends the session is empty, so invoke `done` without calling `iterator`.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(done));
}
