// Wraps a FileSystemObserver to collect its records until it stops receiving
// them.
//
// To collect records, it sets up a directory to observe and periodically create
// files in it. If no new changes occur (outside of these file creations)
// between two file changes, then it resolves the promise returned by
// getRecords() with the records it collected.
class CollectingFileSystemObserver {
  #observer = new FileSystemObserver(this.#collectRecordsCallback.bind(this));

  #records_promise_and_resolvers = Promise.withResolvers();
  #collected_records = [];

  #notification_dir_handle;
  #notification_file_count = 0;
  #received_changes_since_last_notification = true;

  constructor(test, root_dir) {
    test.add_cleanup(() => {
      this.disconnect();
    });

    this.#setupCollectNotification(root_dir);
  }

  #getCollectNotificationName() {
    return `notification_file_${this.#notification_file_count}`;
  }

  async #setupCollectNotification(root_dir) {
    this.#notification_dir_handle =
        await root_dir.getDirectoryHandle(getUniqueName(), {create: true});
    await this.#observer.observe(this.#notification_dir_handle);
    await this.#createCollectNotification();
  }

  #createCollectNotification() {
    this.#notification_file_count++;
    return this.#notification_dir_handle.getFileHandle(
        this.#getCollectNotificationName(), {create: true});
  }

  #finishCollectingIfReady() {
    // `records` contains the notification for collecting records. Determine
    // if we should finish collecting or create the next notification.
    if (this.#received_changes_since_last_notification) {
      this.#received_changes_since_last_notification = false;
      this.#createCollectNotification();
    } else {
      this.#records_promise_and_resolvers.resolve(this.#collected_records);
    }
  }

  #groupRecords(records) {
    return Object.groupBy(records, record => {
      if (record.relativePathComponents[0] ==
          this.#getCollectNotificationName()) {
        return 'notification';
      } else {
        return 'nonNotifications';
      }
    });
  }

  #collectRecordsCallback(records) {
    const {notification, nonNotifications} = this.#groupRecords(records);

    if (nonNotifications) {
      this.#collected_records.push(...nonNotifications);

      this.#received_changes_since_last_notification = true;
    }

    if (notification) {
      this.#finishCollectingIfReady(records)
    }
  }

  getRecords() {
    return this.#records_promise_and_resolvers.promise;
  }

  observe(handles, options) {
    return Promise.all(
        handles.map(handle => this.#observer.observe(handle, options)));
  }

  disconnect() {
    this.#observer.disconnect();
  }
}

async function assert_records_equal(root, actual, expected) {
  assert_equals(
      actual.length, expected.length,
      'Received an unexpected number of events');

  for (let i = 0; i < actual.length; i++) {
    const actual_record = actual[i];
    const expected_record = expected[i];

    assert_equals(
        actual_record.type, expected_record.type,
        'A record\'s type didn\'t match the expected type');

    assert_array_equals(
        actual_record.relativePathComponents,
        expected_record.relativePathComponents,
        'A record\'s relativePathComponents didn\'t match the expected relativePathComponents');

    if (expected_record.relativePathMovedFrom) {
      assert_array_equals(
          actual_record.relativePathMovedFrom,
          expected_record.relativePathMovedFrom,
          'A record\'s relativePathMovedFrom didn\'t match the expected relativePathMovedFrom');
    } else {
      assert_equals(
          actual_record.relativePathMovedFrom, null,
          'A record\'s relativePathMovedFrom was set when it shouldn\'t be');
    }

    assert_true(
        await actual_record.changedHandle.isSameEntry(
            expected_record.changedHandle),
        'A record\'s changedHandle didn\'t match the expected changedHandle');
    assert_true(
        await actual_record.root.isSameEntry(root),
        'A record\'s root didn\'t match the expected root');
  }
}

function modifiedEvent(changedHandle, relativePathComponents) {
  return {type: 'modified', changedHandle, relativePathComponents};
}

function appearedEvent(changedHandle, relativePathComponents) {
  return {type: 'appeared', changedHandle, relativePathComponents};
}

function disappearedEvent(changedHandle, relativePathComponents) {
  return {type: 'disappeared', changedHandle, relativePathComponents};
}
