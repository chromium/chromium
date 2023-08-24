# How to get stack trace for Chrome crashes on DUT builders

This doc is helpful for getting stack trace for skylab builders. It should
be the same process for both Ash and Lacros. Also it should be the same process
for Tast tests, Crosier and gtests.

## From the builder page

It should look like this
![shard]
Then you click the shard link to get to the chromeos builder page.

## Get the binary

On this page, on the right side 'Input properties', you should see the
'lacrosGcsPath'.
![gcs_path]
That path contains all the Chrome files that needed to run the test.

Note: For ash builders, the path name is still 'lacrosGcsPath'.

Download the files locally:
```
$ gsutil cp -r <lacros_gcs_path> ~/
```

## Get the dmp file
Click the testhaus link
![testhaus]
you should see the testhaus page which contains all the logs.
![dmp_file]
You need to find out the right dmp file. Usually if you're only debugging
1 test failure, you will see 2 dmp files due to test retries. You can randomly
select one to download.

## Get stack trace
This is the example commandline:
```
$ sudo mount lacros_compressed.squash /mnt
$ lldb /mnt/out/Release/chromeos_integration_tests -c ~/Downloads/<some_name>.dmp
(lldb) bt
```

For Tast test, replace the target from chromeos_integration_tests to chrome.

Note: Make sure you use the matching binary and dmp file. e.g. You don't want
to use the Ash chrome binary with a Lacros dmp file.

Final note: This doc is only for getting stack trace for the component under
test. That means if your builder build Lacros and lacros crashes, or build Ash
and Ash crashes, this workflow is helpful. But if your builder build Lacros and
causes Ash crashes, the workflow need to change. In theory you can get the
binary from the OS image under test, and dmp file in the same way and then get
stack trace. The author of this doc have not tried this so please help improve
this doc if you have information on this.

[shard]: images/shard.png
[testhaus]: images/testhaus.png
[dmp_file]: images/dmp_file.png
[gcs_path]: images/gcs_path.png
