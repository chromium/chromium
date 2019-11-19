importScripts('../resources/fs-worker-common.js');
importScripts('../../../resources/js-test.js');
importScripts('../resources/fs-test-util.js');

description('This test tries calling various filesystem functions with null arguments.');

fileSystem = webkitRequestFileSystemSync(self.TEMPORARY, 100);

shouldThrow("fileSystem.root.moveTo(null, 'x')");
shouldThrow("fileSystem.root.copyTo(null, 'x')");
entry = fileSystem.root.getFile("/test", { create: true });
writer = entry.createWriter();
shouldThrow("writer.write(null)");
entry.remove();

fileSystem = webkitRequestFileSystem(self.TEMPORARY, null);

finishJSTest();
