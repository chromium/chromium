'use strict';

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
