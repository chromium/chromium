importScripts('/resources/testharness.js');
importScripts('resources/sandboxed-fs-test-helpers.js');
importScripts('resources/test-helpers.js');

'use strict';

// Adds tests to test the interaction between a lock created by the move
// operation and a lock created by `createLock`.
function generateCrossLockMoveTests(lockName, createLock) {
  generateCrossLockTests(createMoveWithCleanup, createLock, {
    diffFile: `A file with an ongoing move operation does not interfere with` +
        ` ${lockName} on another file`,
    acquireAfterRelease: `After a file has finished moving, that file can` +
        ` have ${lockName}`,
    // TODO(https://github.com/whatwg/fs/pull/10): Add tests for directory moves
    // once supported.
  });

  directory_test(async (t, rootDir) => {
    const [fooFileHandle, barFileHandle] =
        await createFileHandles(rootDir, 'foo.test', 'bar.test');

    createLock(t, fooFileHandle);
    await promise_rejects_dom(
        t, 'NoModificationAllowedError',
        createMoveWithCleanup(t, barFileHandle, 'foo.test'));
  }, `A file cannot be moved to a location with ${lockName}`);
}

// Adds tests to test the interaction between a lock created by the remove
// operation and a lock created by `createLock`.
function generateCrossLockRemoveTests(lockName, createLock) {
  generateCrossLockTests(createRemoveWithCleanup, createLock, {
    diffFile: `A file with an ongoing remove operation does not interfere` +
        ` with the creation of ${lockName} on another file`,
    acquireAfterRelease: `After a file has finished being removed, that file` +
        ` can have ${lockName}`,
  });
  generateCrossLockTests(createLock, createRemoveWithCleanup, {
    takeFileThenDir: `A directory cannot be removed if it contains a file` +
        ` that has ${lockName}.`,
  });
}

// Adds tests to test the interaction between a lock created by an open writable
// and a lock created by `createLock`.
function generateCrossLockWFSTests(lockName, createLock) {
  generateCrossLockTests(createWritableWithCleanup, createLock, {
    sameFile: `When there's an open writable stream on a file, cannot have a` +
        ` ${lockName} on that same file`,
    diffFile: `A writable stream from one file does not interfere with a` +
        ` ${lockName} on another file`,
    multiAcquireAfterRelease: `After all writable streams have been closed` +
        ` for a file, that file can have ${lockName}`,
  });
}

// Adds tests to test the interaction between a lock created by an open access
// handle in `sahMode and locks created by other file primitives and operations.
function generateCrossLockSAHTests(sahMode) {
  const createSAHLock = createSAHWithCleanupFactory({mode: sahMode});
  const SAHLockName = `an open access handle in ${sahMode} mode`;

  // Test interaction between move locks and SAH locks.
  generateCrossLockMoveTests(SAHLockName, createSAHLock);
  generateCrossLockTests(createSAHLock, createMoveWithCleanup, {
    sameFile: `A file with ${SAHLockName} cannot be moved`,
    diffFile: `A file with ${SAHLockName} does not interfere with moving` +
        ` another file`,
    acquireAfterRelease: `After ${SAHLockName} on a file has been closed,` +
        ` that file can be moved`,
  });

  // Test interaction between remove locks and SAH locks.
  generateCrossLockRemoveTests(SAHLockName, createSAHLock);
  generateCrossLockTests(createSAHLock, createRemoveWithCleanup, {
    sameFile: `A file with ${SAHLockName} cannot be removed`,
    diffFile: `A file with ${SAHLockName} does not interfere with removing` +
        ` another file`,
    acquireAfterRelease: `After ${SAHLockName} on a file has been closed,` +
        ` that file can be removed`,
  });

  // Test interaction between WFS locks and SAH locks.
  generateCrossLockWFSTests(SAHLockName, createSAHLock);
  generateCrossLockTests(createSAHLock, createWritableWithCleanup, {
    sameFile: `When there's ${SAHLockName} on a file, cannot open a writable` +
        ` stream on that same file`,
    diffFile: `A file with ${SAHLockName} does not interfere with the` +
        ` creation of a writable stream on another file`
  });
}

generateCrossLockSAHTests('readwrite');
generateCrossLockSAHTests('read-only');
generateCrossLockSAHTests('readwrite-unsafe');

// Test interaction between move locks and WFS locks.
generateCrossLockMoveTests(
  'an open writable stream', createWritableWithCleanup);
generateCrossLockWFSTests('an ongoing move operation', createMoveWithCleanup);

// Test interaction between remove locks and WFS locks.
generateCrossLockRemoveTests(
    'an open writable stream', createWritableWithCleanup);
generateCrossLockWFSTests(
    'an ongoing remove operation', createRemoveWithCleanup);

done();
