async function cleanupSandboxedFileSystem() {
    const dir = await FileSystemDirectoryHandle.getSystemDirectory({ type: 'sandbox' });
    for await (let entry of dir.getEntries()) {
        if (entry.isDirectory)
            await entry.removeRecursively();
        else
            await entry.remove();
   }
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
    for await (let entry of handle.getEntries()) {
        result++;
    }
    return result;
}

async function getSortedDirectoryEntries(handle) {
    let result = [];
    for await (let entry of handle.getEntries()) {
        if (entry.isDirectory)
            result.push(entry.name + '/');
        else
            result.push(entry.name);
    }
    result.sort();
    return result;
}

async function createEmptyFile(test, name, parent) {
    const dir = parent ? parent : await FileSystemDirectoryHandle.getSystemDirectory({ type: 'sandbox' });
    const handle = await dir.getFile(name, { create: true });
    test.add_cleanup(async () => {
        try {
            await handle.remove();
        } catch (e) {
            // Ignore any errors when removing files, as tests might already remove the file.
        }
    });
    // Make sure the file is empty.
    assert_equals(await getFileSize(handle), 0);
    return handle;
}

async function createFileWithContents(test, name, contents, parent) {
    const handle = await createEmptyFile(test, name, parent);
    const writer = await handle.createWriter();
    await writer.write(0, new Blob([contents]));
    return handle;
}

function garbageCollect() {
    if (self.gc) {
        // Use --expose_gc for V8 (and Node.js)
        // Exposed in SpiderMonkey shell as well
        self.gc();
    } else if (self.GCController) {
        // Present in some WebKit development environments
        GCController.collect();
    } else {
        /* eslint-disable no-console */
        console.warn('Tests are running without the ability to do manual garbage collection. They will still work, but ' +
            'coverage will be suboptimal.');
        /* eslint-enable no-console */
    }
};