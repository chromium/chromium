# Moving large files to Google Storage

## Problem

There are large binary files checked into our source tree. When we used SVN,
this was suboptimal but bearable because everyone only had the top level
checkout of the files.

As we switch to git, the problem becomes worse because now every will have every
large binary ever checked in.

## Solution

Hash the large files, check in the large files into Google Storage, check the
hashes into the repository.  When we need to fetch the files, just run a script.

We now have two tools to help with this process in depot_tools:
`depot_tools/download_from_google_storage`, and
`depot_tools/upload_to_google_storage.py`.

## Steps

### Step 1. Create a Google Storage bucket

For Android-related files, it might be appropriate to use the shared
chromium-android-tools bucket. Create a new folder in the bucket, and use
chromium-android-tools/[new folder name] as the bucket name.

Otherwise, go to [go/chromeinfraticket](http://go/chromeinfraticket) to request
a bucket.  We ask that you do this so we can ensure the ACLs will work on our
buildbots, and storage costs will be centralize to Chrome Infrastructure.
You'll need to specify:

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

Once you have your shiny new bucket, run:

```
$ upload_to_google_storage.py --bucket chromium-my-bucket-name ./file-I-want-to-upload
Main> Calculating hash for ./file-I-want-to-upload...
Main> Done calculating hash for ./file-I-want-to-upload.
0> Uploading ./file-I-want-to-upload...
Hashing 1 files took 0.285466 seconds
Uploading took 256.985357 seconds
Success!
```

If you have more than 1 file, you can pipe it into upload_to_google_storage.py,
and set the target to "-" (without the quotes)

```bash
$ find . -name *exe | upload_to_google_storage.py --bucket chromium-my-bucket-name -
```

If there are spaces in the name, use a null terminator and pass in
--null_terminator into the script

```bash
$ find . -name *exe -print0 | upload_to_google_storage.py --bucket chromium-my-bucket-name --null_terminator -
$ find . -name *exe -print0 | upload_to_google_storage.py -b chromium-my-bucket-name -0 - # Shorthand
```

### Step 4. Check the .sha1 files into repository

Step 3 creates a .sha1 file for each uploaded file, with just the sha1 sum of
the file.  We'll want to check this into the repository.

If the files are already check into the repository, we can remove them now.

```bash
$ git add ./file-I-want-to-upload.sha1
$ git rm ./file-I-want-to-upload  # If the file was previously in the repo.
```

### Step 5. Add a hook in your DEPS file to fetch the files:

Add this to the top level .DEPS file.  This can also go in the .gclient file if
you want people to only fetch the files if the specifically add it.

See download_from_google_storage --help for all available flags.  The following
is to scan a folder for all .sha1 files. Other configs can be found
[here][1]

```python
hooks = [
  {
    "pattern": "\\.sha1",                    # Only update when a .sha1 file has been modified.
    "action": [ "download_from_google_storage",
    "--directory",
    "--recursive",
    "--bucket", "chromium-my-bucket-name",  # Replace with your bucket name.
    "./chrome/path/to/directory"]           # Replace with the root folder to scan from.  Relative path from .gclient file.
  }
]
```

[1]: https://code.google.com/p/chromium/codesearch#chromium/src/DEPS&sq=package:chromium&q=DEPS&l=723

### Step 6. Add the file names to .gitignore.

This is so that the files do not show up in a git status.
