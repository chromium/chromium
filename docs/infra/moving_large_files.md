# Moving large files to Google Storage

## Problem

There are large binary files checked into our source tree. When we used SVN,
this was suboptimal but bearable because everyone only had the top level
checkout of the files.

As we switch to git, the problem becomes worse because now every will have every
large binary ever checked in.

## Solution

Hash the large files, check in the large files into Google Storage, add the GCS
object as a dependency in the DEPS file. Files will be fetched from Google
Storage during `gclient sync`.

## Steps

### Step 1. Create a Google Storage bucket

For Android-related files, it might be appropriate to use the shared
chromium-android-tools bucket. Create a new folder in the bucket, and use
chromium-android-tools/[new folder name] as the bucket name.

Otherwise, to create a new bucket you can add an entry into
[bucket.json](http://shortn/_wkOtCQm10E). Entries within `bucket.json` should
aim to answer the below questions. Afterwards, send the CL to a coworker for review.
After that is approved, someone from chops security will do a final review.

* Who can have read access to this bucket? Certain groups at Google? All of
  Google? All of Chrome-Team? Everyone? Consider adding googlers@chromium.org to
  private buckets for those who use their @chromium.org account for auth with
  depot_tools.
* Who can have read/write access to this bucket? Certain groups at Google
  (@google.com group)? All of Chrome-team? All of Google?
* Give this bucket a name.  "chromium-something" is good for public buckets,
  "chrome-something" is good for private buckets.

### Step 2. Set up gcloud auth tokens with your @google.com account
(not required if already authenticated)

*IMPORTANT: Make sure you use your @google.com account.*

```
$ download_from_google_storage --config
This command will create a boto config file at
/usr/local/google/home/magjed/.boto.depot_tools containing your
credentials, based on your responses to the following questions.
Please navigate your browser to the following URL:
https://accounts.google.com/o/oauth2/auth?<...snip...>
Enter the authorization code: <Enter code here>
Please navigate your browser to https://cloud.google.com/console#/project,
then find the project you will use, and copy the Project ID string from the
second column. Older projects do not have Project ID strings. For such
projects, click the project and then copy the Project Number listed under that
project.
What is your project-id? <Enter project ID here>
Boto config file "/usr/local/google/home/magjed/.boto.depot_tools"
created.
```

You will be asked for "project-id", type whatever you want there, "browser" is
typical.

### Step 3. Check your files into the bucket

Once you have your shiny new bucket, follow the
[instructions](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/gcs_dependencies.md#upload-a-new-object-to-gcs)
for adding GCS objects as a dependency in the DEPS file.

### Step 4. Add the file names to .gitignore.

This is so that the files do not show up in a git status.
