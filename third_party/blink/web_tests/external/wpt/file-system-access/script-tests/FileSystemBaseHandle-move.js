// META: script=resources/test-helpers.js

'use strict';

directory_test(async (t, root) => {
  const handle = await createFileWithContents(t, 'file-before', 'foo', root);
  await handle.move(root, 'file-after');

  assert_array_equals(await getSortedDirectoryEntries(root), ['file-after']);
  assert_equals(await getFileContents(handle), 'foo');
  assert_equals(await getFileSize(handle), 3);
}, 'move(dir, name) to rename a file');

directory_test(async (t, root) => {
  const handle = await createFileWithContents(t, 'file-before', 'foo', root);
  await handle.move(root, 'file-before');

  assert_array_equals(await getSortedDirectoryEntries(root), ['file-before']);
  assert_equals(await getFileContents(handle), 'foo');
  assert_equals(await getFileSize(handle), 3);
}, 'move(dir, name) to rename a file the same name');

directory_test(async (t, root) => {
  const dir = await root.getDirectoryHandle('dir-before', {create: true});
  await dir.move(root, 'dir-after');

  assert_array_equals(await getSortedDirectoryEntries(root), ['dir-after/']);
  assert_array_equals(await getSortedDirectoryEntries(dir), []);
}, 'move(dir, name) to rename an empty directory');

directory_test(async (t, root) => {
  const dir = await root.getDirectoryHandle('dir-before', {create: true});
  await createFileWithContents(t, 'file-in-dir', 'abc', dir);
  await dir.move(root, 'dir-after');

  assert_array_equals(await getSortedDirectoryEntries(root), ['dir-after/']);
  assert_array_equals(await getSortedDirectoryEntries(dir), ['file-in-dir']);
}, 'move(dir, name) to rename a non-empty directory');

directory_test(async (t, root) => {
  const dir_src = await root.getDirectoryHandle('dir-src', {create: true});
  const dir_dest = await root.getDirectoryHandle('dir-dest', {create: true});
  const file = await createFileWithContents(t, 'file', 'abc', dir_src);
  await file.move(dir_dest);

  assert_array_equals(
      await getSortedDirectoryEntries(root), ['dir-dest/', 'dir-src/']);
  assert_array_equals(await getSortedDirectoryEntries(dir_src), []);
  assert_array_equals(await getSortedDirectoryEntries(dir_dest), ['file']);
  assert_equals(await getFileContents(file), 'abc');
  assert_equals(await getFileSize(file), 3);
}, 'move(dir) to move a file to a new directory');

directory_test(async (t, root) => {
  const dir_src = await root.getDirectoryHandle('dir-src', {create: true});
  const dir_dest = await root.getDirectoryHandle('dir-dest', {create: true});
  const file = await createFileWithContents(t, 'file', 'abc', dir_src);
  await file.move(dir_dest, '');

  assert_array_equals(
      await getSortedDirectoryEntries(root), ['dir-dest/', 'dir-src/']);
  assert_array_equals(await getSortedDirectoryEntries(dir_src), []);
  assert_array_equals(await getSortedDirectoryEntries(dir_dest), ['file']);
  assert_equals(await getFileContents(file), 'abc');
  assert_equals(await getFileSize(file), 3);
}, 'move(dir, "") to move a file to a new directory');

directory_test(async (t, root) => {
  const dir_src = await root.getDirectoryHandle('dir-src', {create: true});
  const dir_dest = await root.getDirectoryHandle('dir-dest', {create: true});
  const file =
      await createFileWithContents(t, 'file-in-dir-src', 'abc', dir_src);
  await file.move(dir_dest, 'file-in-dir-dest');

  assert_array_equals(
      await getSortedDirectoryEntries(root), ['dir-dest/', 'dir-src/']);
  assert_array_equals(await getSortedDirectoryEntries(dir_src), []);
  assert_array_equals(
      await getSortedDirectoryEntries(dir_dest), ['file-in-dir-dest']);
  assert_equals(await getFileContents(file), 'abc');
  assert_equals(await getFileSize(file), 3);
}, 'move(dir, name) to move a file to a new directory');

directory_test(async (t, root) => {
  const dir_src = await root.getDirectoryHandle('dir-src', {create: true});
  const dir_dest = await root.getDirectoryHandle('dir-dest', {create: true});
  const dir_in_dir =
      await dir_src.getDirectoryHandle('dir-in-dir', {create: true});
  await dir_in_dir.move(dir_dest);

  assert_array_equals(
      await getSortedDirectoryEntries(root), ['dir-dest/', 'dir-src/']);
  assert_array_equals(await getSortedDirectoryEntries(dir_src), []);
  assert_array_equals(
      await getSortedDirectoryEntries(dir_dest), ['dir-in-dir/']);
  assert_array_equals(await getSortedDirectoryEntries(dir_in_dir), []);
}, 'move(dir) to move an empty directory to a new directory');

directory_test(async (t, root) => {
  const dir_src = await root.getDirectoryHandle('dir-src', {create: true});
  const dir_dest = await root.getDirectoryHandle('dir-dest', {create: true});
  const dir_in_dir =
      await dir_src.getDirectoryHandle('dir-in-dir', {create: true});
  await dir_in_dir.move(dir_dest, "");

  assert_array_equals(
      await getSortedDirectoryEntries(root), ['dir-dest/', 'dir-src/']);
  assert_array_equals(await getSortedDirectoryEntries(dir_src), []);
  assert_array_equals(
      await getSortedDirectoryEntries(dir_dest), ['dir-in-dir/']);
  assert_array_equals(await getSortedDirectoryEntries(dir_in_dir), []);
}, 'move(dir, "") to move an empty directory to a new directory');

directory_test(async (t, root) => {
  const dir_src = await root.getDirectoryHandle('dir-src', {create: true});
  const dir_dest = await root.getDirectoryHandle('dir-dest', {create: true});
  const dir_in_dir =
      await dir_src.getDirectoryHandle('dir-in-dir', {create: true});
  await dir_in_dir.move(dir_dest, 'dir-in-dir');

  assert_array_equals(
      await getSortedDirectoryEntries(root), ['dir-dest/', 'dir-src/']);
  assert_array_equals(await getSortedDirectoryEntries(dir_src), []);
  assert_array_equals(
      await getSortedDirectoryEntries(dir_dest), ['dir-in-dir/']);
  assert_array_equals(await getSortedDirectoryEntries(dir_in_dir), []);
}, 'move(dir, name) to move an empty directory to a new directory');

directory_test(async (t, root) => {
  const dir_src = await root.getDirectoryHandle('dir-src', {create: true});
  const dir_dest = await root.getDirectoryHandle('dir-dest', {create: true});
  const dir_in_dir =
      await dir_src.getDirectoryHandle('dir-in-dir', {create: true});
  const file =
      await createFileWithContents(t, 'file-in-dir', 'abc', dir_in_dir);
  await dir_in_dir.move(dir_dest, "");

  assert_array_equals(
      await getSortedDirectoryEntries(root), ['dir-dest/', 'dir-src/']);
  assert_array_equals(await getSortedDirectoryEntries(dir_src), []);
  assert_array_equals(
      await getSortedDirectoryEntries(dir_dest), ['dir-in-dir/']);
  assert_array_equals(
      await getSortedDirectoryEntries(dir_in_dir), ['file-in-dir']);
  // `file` should be invalidated after moving directories.
  await promise_rejects_dom(t, 'NotFoundError', getFileContents(file));
}, 'move(dir, "") to move a non-empty directory to a new directory');

directory_test(async (t, root) => {
  const dir_src = await root.getDirectoryHandle('dir-src', {create: true});
  const dir_dest = await root.getDirectoryHandle('dir-dest', {create: true});
  const dir_in_dir =
      await dir_src.getDirectoryHandle('dir-in-dir', {create: true});
  const file =
      await createFileWithContents(t, 'file-in-dir', 'abc', dir_in_dir);
  await dir_in_dir.move(dir_dest, 'dir-in-dir');

  assert_array_equals(
      await getSortedDirectoryEntries(root), ['dir-dest/', 'dir-src/']);
  assert_array_equals(await getSortedDirectoryEntries(dir_src), []);
  assert_array_equals(
      await getSortedDirectoryEntries(dir_dest), ['dir-in-dir/']);
  assert_array_equals(
      await getSortedDirectoryEntries(dir_in_dir), ['file-in-dir']);
  // `file` should be invalidated after moving directories.
  await promise_rejects_dom(t, 'NotFoundError', getFileContents(file));
}, 'move(dir, name) to move a non-empty directory to a new directory');

directory_test(async (t, root) => {
  const dir1 = await root.getDirectoryHandle('dir1', {create: true});
  const dir2 = await root.getDirectoryHandle('dir2', {create: true});
  const handle = await createFileWithContents(t, 'file', 'foo', root);

  await handle.move(dir1);
  assert_array_equals(
      await getSortedDirectoryEntries(root), ['dir1/', 'dir2/']);
  assert_array_equals(await getSortedDirectoryEntries(dir1), ['file']);
  assert_array_equals(await getSortedDirectoryEntries(dir2), []);
  assert_equals(await getFileContents(handle), 'foo');

  await handle.move(dir2);
  assert_array_equals(
      await getSortedDirectoryEntries(root), ['dir1/', 'dir2/']);
  assert_array_equals(await getSortedDirectoryEntries(dir1), []);
  assert_array_equals(await getSortedDirectoryEntries(dir2), ['file']);
  assert_equals(await getFileContents(handle), 'foo');

  await handle.move(root);
  assert_array_equals(
      await getSortedDirectoryEntries(root), ['dir1/', 'dir2/', 'file']);
  assert_array_equals(await getSortedDirectoryEntries(dir1), []);
  assert_array_equals(await getSortedDirectoryEntries(dir2), []);
  assert_equals(await getFileContents(handle), 'foo');
}, 'move(dir) can be called multiple times');

directory_test(async (t, root) => {
  const dir1 = await root.getDirectoryHandle('dir1', {create: true});
  const dir2 = await root.getDirectoryHandle('dir2', {create: true});
  const handle = await createFileWithContents(t, 'file', 'foo', root);

  await handle.move(dir1, "");
  assert_array_equals(
      await getSortedDirectoryEntries(root), ['dir1/', 'dir2/']);
  assert_array_equals(await getSortedDirectoryEntries(dir1), ['file']);
  assert_array_equals(await getSortedDirectoryEntries(dir2), []);
  assert_equals(await getFileContents(handle), 'foo');

  await handle.move(dir2, "");
  assert_array_equals(
      await getSortedDirectoryEntries(root), ['dir1/', 'dir2/']);
  assert_array_equals(await getSortedDirectoryEntries(dir1), []);
  assert_array_equals(await getSortedDirectoryEntries(dir2), ['file']);
  assert_equals(await getFileContents(handle), 'foo');

  await handle.move(root, "");
  assert_array_equals(
      await getSortedDirectoryEntries(root), ['dir1/', 'dir2/', 'file']);
  assert_array_equals(await getSortedDirectoryEntries(dir1), []);
  assert_array_equals(await getSortedDirectoryEntries(dir2), []);
  assert_equals(await getFileContents(handle), 'foo');
}, 'move(dir, "") can be called multiple times');

directory_test(async (t, root) => {
  const dir1 = await root.getDirectoryHandle('dir1', {create: true});
  const dir2 = await root.getDirectoryHandle('dir2', {create: true});
  const handle = await createFileWithContents(t, 'file', 'foo', root);

  await handle.move(dir1, 'file-1');
  assert_array_equals(
      await getSortedDirectoryEntries(root), ['dir1/', 'dir2/']);
  assert_array_equals(await getSortedDirectoryEntries(dir1), ['file-1']);
  assert_array_equals(await getSortedDirectoryEntries(dir2), []);
  assert_equals(await getFileContents(handle), 'foo');

  await handle.move(dir2, 'file-2');
  assert_array_equals(
      await getSortedDirectoryEntries(root), ['dir1/', 'dir2/']);
  assert_array_equals(await getSortedDirectoryEntries(dir1), []);
  assert_array_equals(await getSortedDirectoryEntries(dir2), ['file-2']);
  assert_equals(await getFileContents(handle), 'foo');

  await handle.move(root, 'file-3');
  assert_array_equals(
      await getSortedDirectoryEntries(root), ['dir1/', 'dir2/', 'file-3']);
  assert_array_equals(await getSortedDirectoryEntries(dir1), []);
  assert_array_equals(await getSortedDirectoryEntries(dir2), []);
  assert_equals(await getFileContents(handle), 'foo');
}, 'move(dir, name) can be called multiple times');

directory_test(async (t, root) => {
  const handle = await createFileWithContents(t, 'file-before', 'foo', root);
  await promise_rejects_js(
      t, TypeError, handle.move(root, '#$23423@352^*3243'));

  assert_array_equals(await getSortedDirectoryEntries(root), ['file-before']);
  assert_equals(await getFileContents(handle), 'foo');
  assert_equals(await getFileSize(handle), 3);
}, 'move(dir, name) with a name with invalid characters should fail');

directory_test(async (t, root) => {
  const dir = await root.getDirectoryHandle('dir', {create: true});
  await promise_rejects_dom(
      t, 'InvalidModificationError', dir.move(dir));

  assert_array_equals(await getSortedDirectoryEntries(root), ['dir/']);
  assert_array_equals(await getSortedDirectoryEntries(dir), []);
}, 'move(dir, name) to move a directory within itself fails');

directory_test(async (t, root) => {
  const dir = await root.getDirectoryHandle('dir', {create: true});
  await promise_rejects_dom(
      t, 'InvalidModificationError', dir.move(dir, 'dir-fail'));

  assert_array_equals(await getSortedDirectoryEntries(root), ['dir/']);
  assert_array_equals(await getSortedDirectoryEntries(dir), []);
}, 'move(dir, name) to move a directory within itself and rename fails');

directory_test(async (t, root) => {
  const parent_dir =
      await root.getDirectoryHandle('parent-dir', {create: true});
  const child_dir =
      await parent_dir.getDirectoryHandle('child-dir', {create: true});
  await promise_rejects_dom(
      t, 'InvalidModificationError', parent_dir.move(child_dir));

  assert_array_equals(await getSortedDirectoryEntries(root), ['parent-dir/']);
  assert_array_equals(
      await getSortedDirectoryEntries(parent_dir), ['child-dir/']);
  assert_array_equals(await getSortedDirectoryEntries(child_dir), []);
}, 'move(dir) to move a directory within a descendent fails');

directory_test(async (t, root) => {
  const parent_dir =
      await root.getDirectoryHandle('parent-dir', {create: true});
  const child_dir =
      await parent_dir.getDirectoryHandle('child-dir', {create: true});
  await promise_rejects_dom(
      t, 'InvalidModificationError', parent_dir.move(child_dir, 'dir'));

  assert_array_equals(await getSortedDirectoryEntries(root), ['parent-dir/']);
  assert_array_equals(
      await getSortedDirectoryEntries(parent_dir), ['child-dir/']);
  assert_array_equals(await getSortedDirectoryEntries(child_dir), []);
}, 'move(dir, name) to move a directory within a descendent fails');

directory_test(async (t, root) => {
  const dir_src = await root.getDirectoryHandle('dir-src', {create: true});
  const dir_dest = await root.getDirectoryHandle('dir-dest', {create: true});
  const file = await createFileWithContents(t, 'file', 'abc', dir_src);

  // Cannot move handle with an active writable.
  const stream = await file.createWritable();
  await promise_rejects_dom(t, 'InvalidStateError', file.move(dir_dest));

  assert_array_equals(
      await getSortedDirectoryEntries(root), ['dir-dest/', 'dir-src/']);
  // Assert the file hasn't been moved to the destination directory.
  assert_array_equals(await getSortedDirectoryEntries(dir_dest), []);

  // Can move handle once the writable is closed.
  await stream.close();
  await file.move(dir_dest);
  assert_array_equals(
      await getSortedDirectoryEntries(root), ['dir-dest/', 'dir-src/']);
  assert_array_equals(await getSortedDirectoryEntries(dir_src), []);
  assert_array_equals(await getSortedDirectoryEntries(dir_dest), ['file']);
}, 'move(dir) while the file has an open writable fails');

directory_test(async (t, root) => {
  const dir_src = await root.getDirectoryHandle('dir-src', {create: true});
  const dir_dest = await root.getDirectoryHandle('dir-dest', {create: true});
  const file = await createFileWithContents(t, 'file-before', 'abc', dir_src);

  // Cannot move handle with an active writable.
  const stream = await file.createWritable();
  await promise_rejects_dom(t, 'InvalidStateError', file.move(dir_dest));

  assert_array_equals(
      await getSortedDirectoryEntries(root), ['dir-dest/', 'dir-src/']);
  // Assert the file hasn't been moved to the destination directory.
  assert_array_equals(await getSortedDirectoryEntries(dir_dest), []);

  // Can move handle once the writable is closed.
  await stream.close();
  await file.move(dir_dest, 'file-after');
  assert_array_equals(
      await getSortedDirectoryEntries(root), ['dir-dest/', 'dir-src/']);
  assert_array_equals(await getSortedDirectoryEntries(dir_src), []);
  assert_array_equals(
      await getSortedDirectoryEntries(dir_dest), ['file-after']);
}, 'move(dir, name) while the file has an open writable fails');

directory_test(async (t, root) => {
  const dir_src = await root.getDirectoryHandle('dir-src', {create: true});
  const dir_dest = await root.getDirectoryHandle('dir-dest', {create: true});
  const file = await createFileWithContents(t, 'file', 'abc', dir_src);
  const file_dest = await createFileWithContents(t, 'file', '123', dir_dest);

  // Cannot overwrite handle with an active writable.
  const stream = await file_dest.createWritable();
  await promise_rejects_dom(t, 'InvalidStateError', file.move(dir_dest));

  assert_array_equals(
      await getSortedDirectoryEntries(root), ['dir-dest/', 'dir-src/']);
  // Assert the file is still in the source directory.
  assert_array_equals(await getSortedDirectoryEntries(dir_src), ['file']);

  // Can move handle once the writable is closed.
  await stream.close();
  await file.move(dir_dest);
  assert_array_equals(
      await getSortedDirectoryEntries(root), ['dir-dest/', 'dir-src/']);
  assert_array_equals(await getSortedDirectoryEntries(dir_src), []);
  assert_array_equals(await getSortedDirectoryEntries(dir_dest), ['file']);
  assert_equals(await getFileContents(file), 'abc');
  assert_equals(await getFileSize(file), 3);
}, 'move(dir) while the destination file has an open writable fails');

directory_test(async (t, root) => {
  const dir_src = await root.getDirectoryHandle('dir-src', {create: true});
  const dir_dest = await root.getDirectoryHandle('dir-dest', {create: true});
  const file = await createFileWithContents(t, 'file-src', 'abc', dir_src);
  const file_dest =
      await createFileWithContents(t, 'file-dest', '123', dir_dest);

  // Cannot overwrite handle with an active writable.
  const stream = await file_dest.createWritable();
  await promise_rejects_dom(
      t, 'InvalidStateError', file.move(dir_dest, 'file-dest'));

  assert_array_equals(
      await getSortedDirectoryEntries(root), ['dir-dest/', 'dir-src/']);
  // Assert the file is still in the source directory.
  assert_array_equals(await getSortedDirectoryEntries(dir_src), ['file-src']);

  // Can move handle once the writable is closed.
  await stream.close();
  await file.move(dir_dest, 'file-dest');
  assert_array_equals(
      await getSortedDirectoryEntries(root), ['dir-dest/', 'dir-src/']);
  assert_array_equals(await getSortedDirectoryEntries(dir_src), []);
  assert_array_equals(await getSortedDirectoryEntries(dir_dest), ['file-dest']);
  assert_equals(await getFileContents(file), 'abc');
  assert_equals(await getFileSize(file), 3);
}, 'move(dir, name) while the destination file has an open writable fails');
