(async function (/** @type {import('test_runner').TestRunner} */ testRunner) {
    const { page, session, dp } = await testRunner.startBlank(
        `Tests retrieving nested directory from the protocol.`);

    await dp.Runtime.enable();

    await session.evaluateAsync(`
        new Promise(async resolve => {
            const generateFilesAndDirectoriesForBucket = async(bucketName = undefined) => {

                let root;
                let prefix = "";

                if(bucketName === undefined) {
                    root = await navigator.storage.getDirectory();
                }
                else {
                    const bucket = await navigator.storageBuckets.open(bucketName, { durability: "strict", persist: false });
                    root = await bucket.getDirectory();
                    prefix = bucketName + ":";
                }

                await root.getFileHandle("root.txt", { create: true });

                const firstLevelDirectory = await root.getDirectoryHandle(prefix + 'first_level', { create: true });
                await firstLevelDirectory.getFileHandle(prefix + 'first_level.txt', { create: true });

                const secondLevelDirectory = await firstLevelDirectory.getDirectoryHandle(prefix + 'second_level', { create: true });
                await secondLevelDirectory.getDirectoryHandle(prefix + 'third_level', { create: true });
                await secondLevelDirectory.getDirectoryHandle(prefix + 'third_level_2', { create: true });
                await secondLevelDirectory.getFileHandle(prefix + 'second_level.txt', { create: true });
                await secondLevelDirectory.getFileHandle(prefix + 'second_level_2.txt', { create: true });
            }

            await generateFilesAndDirectoriesForBucket();
            await generateFilesAndDirectoriesForBucket("test_bucket");

            resolve();
        });
    `);

    const dumpEntries = (directory) => {
        testRunner.log("");
        testRunner.log(`Directory: ${directory.name}`);

        testRunner.log(`Nested Directories:`);
        directory.nestedDirectories.forEach(nestedDirectory => {
            testRunner.log(`  - Name: ${nestedDirectory}`);
        });

        testRunner.log(`Nested Files:`);
        directory.nestedFiles.forEach(nestedFile => {
            testRunner.log(`  - Name: ${nestedFile.name}`);
            testRunner.log(`    Size: ${nestedFile.size}`);
            testRunner.log(`    Type: ${nestedFile.type}`);
        });
    }

    for (const bucketName of [undefined, "test_bucket"]) {
        let prefix = "";

        if (bucketName) {
            prefix = `${bucketName}:`;
        }

        const response = await dp.FileSystem.getDirectory({
            bucketFileSystemLocator: {
                storageKey: "http://127.0.0.1:8000/",
                bucketName: bucketName,
                pathComponents: [prefix + "first_level", prefix + "second_level"],
            }
        })

        dumpEntries(response.result.directory);
    }

    testRunner.completeTest()
});
