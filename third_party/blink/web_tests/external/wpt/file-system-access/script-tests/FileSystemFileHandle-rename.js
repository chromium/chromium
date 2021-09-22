// META: script=resources/test-helpers.js

'use strict';

directory_test(async (t, root) => {
  const handle = await createFileWithContents(t, 'file-before', 'foo', root);
  await handle.rename('file-after');

  assert_array_equals(await getSortedDirectoryEntries(root), ['file-after']);
  assert_equals(await getFileContents(handle), 'foo');
  assert_equals(await getFileSize(handle), 3);
}, 'rename(name) to rename a file');

directory_test(async (t, root) => {
  const handle = await createFileWithContents(t, 'file-before', 'foo', root);
  await handle.rename('file-before');

  assert_array_equals(await getSortedDirectoryEntries(root), ['file-before']);
  assert_equals(await getFileContents(handle), 'foo');
  assert_equals(await getFileSize(handle), 3);
}, 'rename(name) to rename a file the same name');

directory_test(async (t, root) => {
  const handle = await createFileWithContents(t, 'file-before', 'foo', root);
  await promise_rejects_js(t, TypeError, handle.rename(''));

  assert_array_equals(await getSortedDirectoryEntries(root), ['file-before']);
  assert_equals(await getFileContents(handle), 'foo');
  assert_equals(await getFileSize(handle), 3);
}, 'rename("") to rename a file fails');

directory_test(async (t, root) => {
  const handle = await createFileWithContents(t, 'file-1', 'foo', root);

  await handle.rename('file-2');
  assert_array_equals(await getSortedDirectoryEntries(root), ['file-2']);

  await handle.rename('file-3');
  assert_array_equals(await getSortedDirectoryEntries(root), ['file-3']);

  await handle.rename('file-1');
  assert_array_equals(await getSortedDirectoryEntries(root), ['file-1']);
}, 'rename(name) can be called multiple times');

directory_test(async (t, root) => {
  const dir = await root.getDirectoryHandle('dir', {create: true});
  const handle = await createFileWithContents(t, 'file-before', 'foo', dir);
  await handle.rename(root);

  assert_array_equals(await getSortedDirectoryEntries(root), ['dir/']);
  assert_array_equals(
      await getSortedDirectoryEntries(dir),
      ['[object FileSystemDirectoryHandle]']);
  assert_equals(await getFileContents(handle), 'foo');
  assert_equals(await getFileSize(handle), 3);
}, 'rename(dir) should rename to stringified dir object');

directory_test(async (t, root) => {
  const dir = await root.getDirectoryHandle('dir', {create: true});
  const handle = await createFileWithContents(t, 'file-before', 'foo', dir);
  await promise_rejects_js(t, TypeError, handle.rename('Lorem.'));

  assert_array_equals(await getSortedDirectoryEntries(root), ['dir/']);
  assert_array_equals(await getSortedDirectoryEntries(dir), ['file-before']);
  assert_equals(await getFileContents(handle), 'foo');
  assert_equals(await getFileSize(handle), 3);
}, 'rename(name) with a name with a trailing period should fail');

directory_test(async (t, root) => {
  const handle = await createFileWithContents(t, 'file-before', 'foo', root);
  await promise_rejects_js(t, TypeError, handle.rename('#$23423@352^*3243'));

  assert_array_equals(await getSortedDirectoryEntries(root), ['file-before']);
  assert_equals(await getFileContents(handle), 'foo');
  assert_equals(await getFileSize(handle), 3);
}, 'rename(name) with a name with invalid characters should fail');

directory_test(async (t, root) => {
  const handle = await createFileWithContents(t, 'file-before', 'abc', root);

  // Cannot rename handle with an active writable.
  const stream = await handle.createWritable();
  await promise_rejects_dom(
      t, 'InvalidStateError', handle.rename('file-after'));

  // Can move handle once the writable is closed.
  await stream.close();
  await handle.rename('file-after');
  assert_array_equals(await getSortedDirectoryEntries(root), ['file-after']);
}, 'rename(name) while the file has an open writable fails');

directory_test(async (t, root) => {
  const handle = await createFileWithContents(t, 'file-before', 'abc', root);
  const handle_dest =
      await createFileWithContents(t, 'file-after', '123', root);

  // Cannot overwrite a handle with an active writable.
  const stream = await handle_dest.createWritable();
  await promise_rejects_dom(
      t, 'InvalidStateError', handle.rename('file-after'));

  // Can move handle once the writable is closed.
  await stream.close();
  await handle.rename('file-after');
  assert_array_equals(await getSortedDirectoryEntries(root), ['file-after']);
}, 'rename(name) while the destination file has an open writable fails');
