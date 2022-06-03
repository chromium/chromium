const path =
    String.raw`{{fs_path(resources/data)}}`.replace('wpt_automation', 'wpt');
testRunner.setFilePathForMockFileDialog(path);
