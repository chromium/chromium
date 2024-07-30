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
