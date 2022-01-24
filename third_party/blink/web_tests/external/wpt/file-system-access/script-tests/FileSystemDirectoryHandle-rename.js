// META: script=resources/test-helpers.js

'use strict';

directory_test(async (t, root) => {
  const dir = await root.getDirectoryHandle('dir-before', {create: true});
  await dir.rename('dir-after');

  assert_array_equals(await getSortedDirectoryEntries(root), ['dir-after/']);
  assert_array_equals(await getSortedDirectoryEntries(dir), []);
}, 'rename(name) to rename an empty directory');

directory_test(async (t, root) => {
  const dir = await root.getDirectoryHandle('dir-before', {create: true});
  await promise_rejects_js(t, TypeError, dir.rename(''));

  assert_array_equals(await getSortedDirectoryEntries(root), ['dir-before/']);
  assert_array_equals(await getSortedDirectoryEntries(dir), []);
}, 'rename("") to rename an empty directory fails');

directory_test(async (t, root) => {
  const dir = await root.getDirectoryHandle('dir-before', {create: true});
  await createFileWithContents(t, 'file-in-dir', 'abc', dir);
  await dir.rename('dir-after');

  assert_array_equals(await getSortedDirectoryEntries(root), ['dir-after/']);
  assert_array_equals(await getSortedDirectoryEntries(dir), ['file-in-dir']);
}, 'rename(name) to rename a non-empty directory');
