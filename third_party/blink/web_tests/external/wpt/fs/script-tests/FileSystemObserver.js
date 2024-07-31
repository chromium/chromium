'use strict';

// This script depends on the following scripts:
//    resources/test-helpers.js
//    resources/collecting-file-system-observer.js
//    resources/change-observer-scope-test.js
//    script-tests/FileSystemObserver-writable-file-stream.js

promise_test(async t => {
  try {
    const observer = new FileSystemObserver(() => {});
  } catch {
    assert_unreached();
  }
}, 'Creating a FileSystemObserver from a supported global succeeds');

directory_test(async (t, root_dir) => {
  const observer = new FileSystemObserver(() => {});
  try {
    observer.unobserve(root_dir);
  } catch {
    assert_unreached();
  }
}, 'Calling unobserve() without a corresponding observe() shouldn\'t throw');

directory_test(async (t, root_dir) => {
  const observer = new FileSystemObserver(() => {});
  try {
    observer.unobserve(root_dir);
    observer.unobserve(root_dir);
  } catch {
    assert_unreached();
  }
}, 'unobserve() is idempotent');

promise_test(async t => {
  const observer = new FileSystemObserver(() => {});
  try {
    observer.disconnect();
  } catch {
    assert_unreached();
  }
}, 'Calling disconnect() without observing shouldn\'t throw');

promise_test(async t => {
  const observer = new FileSystemObserver(() => {});
  try {
    observer.disconnect();
    observer.disconnect();
  } catch {
    assert_unreached();
  }
}, 'disconnect() is idempotent');

directory_test(async (t, root_dir) => {
  const observer = new FileSystemObserver(() => {});

  // Create a `FileSystemFileHandle` and delete its underlying file entry.
  const file = await root_dir.getFileHandle(getUniqueName(), {create: true});
  await file.remove();

  await promise_rejects_dom(t, 'NotFoundError', observer.observe(file));
}, 'observe() fails when file does not exist');

directory_test(async (t, root_dir) => {
  const observer = new FileSystemObserver(() => {});

  // Create a `FileSystemDirectoryHandle` and delete its underlying file entry.
  const dir =
      await root_dir.getDirectoryHandle(getUniqueName(), {create: true});
  await dir.remove();

  await promise_rejects_dom(t, 'NotFoundError', observer.observe(dir));
}, 'observe() fails when directory does not exist');

directory_test(async (t, root_dir) => {
  const dir =
      await root_dir.getDirectoryHandle(getUniqueName(), {create: true});

  const scope_test = new ScopeTest(t, dir);
  const watched_handle = await scope_test.watched_handle();

  for (const recursive of [false, true]) {
    for await (const path of scope_test.in_scope_paths(recursive)) {
      const observer = new CollectingFileSystemObserver(t, root_dir);
      await observer.observe([watched_handle], {recursive});

      // Create `file`.
      const file = await path.createHandle();

      // Expect one "appeared" event to happen on `file`.
      const records = await observer.getRecords();
      await assert_records_equal(
          watched_handle, records,
          [appearedEvent(file, path.relativePathComponents())]);

      observer.disconnect();
    }
  }
}, 'Creating a file through FileSystemDirectoryHandle.getFileHandle is reported as an "appeared" event if in scope');

directory_test(async (t, root_dir) => {
  const dir =
      await root_dir.getDirectoryHandle(getUniqueName(), {create: true});

  const scope_test = new ScopeTest(t, dir);
  const watched_handle = await scope_test.watched_handle();

  for (const recursive of [false, true]) {
    for await (const path of scope_test.in_scope_paths(recursive)) {
      const file = await path.createHandle();

      const observer = new CollectingFileSystemObserver(t, root_dir);
      await observer.observe([watched_handle], {recursive});

      // Remove `file`.
      await file.remove();

      // Expect one "disappeared" event to happen on `file`.
      const records = await observer.getRecords();
      await assert_records_equal(
          watched_handle, records,
          [disappearedEvent(file, path.relativePathComponents())]);

      observer.disconnect();
    }
  }
}, 'Removing a file through FileSystemFileHandle.remove is reported as an "disappeared" event if in scope');

directory_test(async (t, root_dir) => {
  const dir =
      await root_dir.getDirectoryHandle(getUniqueName(), {create: true});

  const scope_test = new ScopeTest(t, dir);
  const watched_handle = await scope_test.watched_handle();

  for (const recursive of [false, true]) {
    for await (const path of scope_test.out_of_scope_paths(recursive)) {
      const observer = new CollectingFileSystemObserver(t, root_dir);
      await observer.observe([watched_handle], {recursive});

      // Create and remove `file`.
      const file = await path.createHandle();
      await file.remove();

      // Expect the observer to receive no events.
      const records = await observer.getRecords();
      await assert_records_equal(watched_handle, records, []);

      observer.disconnect();
    }
  }
}, 'Events outside the watch scope are not sent to the observer\'s callback');
