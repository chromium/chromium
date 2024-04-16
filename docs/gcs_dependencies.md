# GCS objects for chromium dependencies

[TOC]

## Summary

You may add GCS objects as dependencies to the chromium checkout via the `deps`
field in the DEPS file. This use-case was previously covered by `hooks` which
makes source and dependency management hard to track. Teams that continue to use
hooks to download from GCS will cause builds to break in certain workflows.

GCS objects can be tar archives, which will automatically be extracted, or
single non-archive files. Whether an object is a tar archive is determined by
[tarfile.is_tarfile](https://docs.python.org/3/library/tarfile.html#tarfile.is_tarfile).

Interrupted downloads or extractions and outdated versions will be detected with
`gclient sync` and trigger re-downlading.

The downloaded content will be validated against the SHA256 content hash and
byte size.

GCS bucket permissions should allow for either allUsers or all googlers to view
the objects within the bucket.

## Adding, uploading, and updating GCS dependencies

### Upload a new object to GCS

There is a helper script
([upload_to_google_storage_first_class.py](https://source.chromium.org/chromium/chromium/tools/depot_tools/+/main:upload_to_google_storage_first_class.py))
to upload new objects to google storage and return the GCS deps entry that
should be copied into DEPS.

### Add/update a GCS entry in DEPS

A GCS entry may be added to the `deps` dict with the following form
([upload_to_google_storage_first_class.py](https://source.chromium.org/chromium/chromium/tools/depot_tools/+/main:upload_to_google_storage_first_class.py)
will also spit out an entry that matches this form):

```
deps = {
  # ...

  # This is the installation directory.
  'src/third_party/blink/renderer/core/css/perftest_data': {
      'bucket': 'chromium-style-perftest',
      'objects': [
        {
          'object_name': '031d5599c8a21118754e30dbea141be66104f556',
          'sha256sum': '031d5599c8a21118754e30dbea141be66104f556',
          'size_bytes': 3203922,
          'generation': 1664794206824773,
          # `output_file` is the name of the file that the downloade object should be
          # saved as. It is optional and only relevant for objects that are NOT tar
          # archives. Tar archives get extracted and saved under the same
          # file/directory names they were archived as.
          'output_file': 'sports.json',
        },
          {
          'object_name': '8aac3db2a8c9e44babec81e539a3d60aeab4985c',
          'sha256sum': '8aac3db2a8c9e44babec81e539a3d60aeab4985c',
          'size_bytes': 5902660,
          'generation': 1664794209886788,
          'output_file': 'video.json',
        },
      ],
      'dep_type' = 'gcs',
  },
}
```

The source of truth for this format is found
[here](https://source.chromium.org/chromium/chromium/tools/depot_tools/+/main:gclient_eval.py;l=135-150?q=gclient_&ss=chromium%2Fchromium%2Ftools%2Fdepot_tools).

#### `size_bytes` and `generation`

If you are not using `upload_to_google_storage_first_class.py` to upload your
objects you can get this information from the command line with:

```
gcloud storage objects describe gs://<bucket>/<object>
```

They can also be found in pantheon when viewing the object's "Object details".
`Size` is found under the `Live Object` tab and `generation` is found under the
`Version History` tab.

#### `sha256sum`

`sha256sum` should be the SHA256 content hash of the GCS object (unextracted).
`upload_to_google_storage_first_class.py` will compute this for you, but if you
are not using that helper script you will have to compute it on your own. You
can test that the hash is correct by running `gclient sync` on your WIP change
that adds the new GCS entry.
