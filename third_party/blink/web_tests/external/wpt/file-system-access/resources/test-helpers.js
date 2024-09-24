// A special path component meaning "this directory."
const kCurrentDirectory = '.';

// A special path component meaning "the parent directory."
const kParentDirectory = '..';

// Array of separators used to separate components in hierarchical paths.
let kPathSeparators;
if (navigator.userAgent.includes('Windows NT')) {
  // Windows uses both '/' and '\' as path separators.
  kPathSeparators = ['/', '\\'];
} else {
  kPathSeparators = ['/'];
}

async function getFileSize(handle) {
  const file = await handle.getFile();
  return file.size;
}

async function getFileContents(handle) {
  const file = await handle.getFile();
  return new Response(file).text();
}

async function getDirectoryEntryCount(handle) {
  let result = 0;
  for await (let entry of handle) {
    result++;
  }
  return result;
}

async function getSortedDirectoryEntries(handle) {
  let result = [];
  for await (let entry of handle.values()) {
    if (entry.kind === 'directory')
      result.push(entry.name + '/');
    else
      result.push(entry.name);
  }
  result.sort();
  return result;
}

async function createDirectory(name, parent) {
  return await parent.getDirectoryHandle(name, {create: true});
}

async function createEmptyFile(name, parent) {
  const handle = await parent.getFileHandle(name, {create: true});
  // Make sure the file is empty.
  assert_equals(await getFileSize(handle), 0);
  return handle;
}

async function createFileWithContents(name, contents, parent) {
  const handle = await createEmptyFile(name, parent);
  const writer = await handle.createWritable();
  await writer.write(new Blob([contents]));
  await writer.close();
  return handle;
}
