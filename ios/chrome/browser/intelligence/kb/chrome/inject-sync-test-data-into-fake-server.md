---
name: ios/inject-sync-test-data-into-fake-server
description: In sync integration tests, seed entities via FakeServer rather than mutating client models, and omit redundant waitForSyncServerEntities calls after injection
rubric: testing
severity: should_fix
citations:
  - path: components/sync/README.md
    heading: Remote Change (Update)
    quote: 3. New data is received by the `DataTypeWorker` and forwarded to the `DataTypeSyncBridge`. 4. The bridge merges the remote changes into the local data model via `ApplyIncrementalSyncChanges()`.
---

**Rule.** In sync integration, UI, and EarlGrey tests, do not populate
sync-backed test data by mutating client models directly while cutting off sync
network access (such as via `FakeServerHttpPostProvider::DisableNetwork()`).
Instead, seed test data by injecting sync entities directly into the sync
`FakeServer` (using `FakeServer::InjectEntity()` or `Add*ToFakeSyncServer` /
`addFakeSyncServer*` helpers). Because `FakeServer` injection is synchronous, do
not follow injection with redundant `waitForSyncServerEntitiesWithType:` calls
(which are only needed for client-to-server uploads); proceed directly to
waiting for or verifying client updates.

**Why.** Directly mutating client models while disabling network access
suppresses sync updates, breaks sync realism, and leaves sync disabled for
subsequent tests. Conversely, injecting entities directly into `FakeServer`
exercises realistic sync delivery and executes synchronously in memory; calling
`waitForSyncServerEntitiesWithType:` after direct injection is redundant,
clutters test code, and inflates test execution time.

## Examples

Bad:

```cpp
// Bad: Mutating client models directly and disabling network transport.
fake_server::FakeServerHttpPostProvider::DisableNetwork();
send_tab_to_self::SendTabToSelfModel* model =
    SendTabToSelfSyncServiceFactory::GetForProfile(profile)->GetSendTabToSelfModel();
model->AddEntry(url, title, target_device_cache_guid);

// Bad: Redundantly waiting for server entities after synchronous FakeServer injection.
NSString* GUID = [ChromeEarlGrey addFakeSendTabToSelfEntryWithURL:URLString
                                                            title:@"Form Page"
                                                    formFieldData:formData];
[ChromeEarlGrey waitForSyncServerEntitiesWithType:syncer::SEND_TAB_TO_SELF
                                             name:"Form Page"
                                            count:1
                                          timeout:base::Seconds(10)];
[ChromeEarlGrey waitForSendTabToSelfEntryWithGUID:GUID];
```

Good:

```cpp
// Good: Injecting entities into FakeServer helper to exercise realistic sync delivery.
chrome_test_util::AddSendTabToSelfEntryToFakeSyncServer(
    url, title, target_device_name, target_device_cache_guid, form_fields);

// Good: Directly waiting for client entry after synchronous FakeServer injection.
NSString* GUID = [ChromeEarlGrey addFakeSendTabToSelfEntryWithURL:URLString
                                                            title:@"Form Page"
                                                    formFieldData:formData];
[ChromeEarlGrey waitForSendTabToSelfEntryWithGUID:GUID];
```
