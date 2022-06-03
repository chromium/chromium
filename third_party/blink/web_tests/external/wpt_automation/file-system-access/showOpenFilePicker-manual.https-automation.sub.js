const path = String.raw`{{fs_path(resources/data/testfile.txt)}}`.replace(
    'wpt_automation', 'wpt');
testRunner.setFilePathForMockFileDialog(path);
