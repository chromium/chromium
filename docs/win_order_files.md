# Updating the Windows .order files

The `chrome/build/*.orderfile` files are used to specify the order in which
the linker should lay out symbols in the binary it's producing. By ordering
functions in the order they're typically executed during start-up, the start-up
time can be reduced slightly.

The order files are used automatically when building with Clang for Windows with
the gn flag `is_official_build` set to `true`.

To update the order files:

1.  Build with instrumentation enabled:

    The instrumentation will capture the couple of million function calls
    in a binary as it runs and write them to a file in the `\src\tmp` directory.
    Make sure this directory exists.

    ```shell
    gn gen out\instrument --args="is_debug=false is_official_build=true generate_order_files=true symbol_level=1"
    ninja -C out\instrument chrome
    ```

    (If you have access to Goma, add `use_goma=true` to the gn args and `-j500`
    to the Ninja invocation.)


1.  Run the instrumented binaries:

    (Some binaries such as `mksnapshot`, `yasm`, and `protoc` already ran with
    instrumentation during the build process. The instrumentation output should
    be available under `\src\tmp`.)

    Open the Task Manager's Details tab or
    [Process Explorer](https://docs.microsoft.com/en-us/sysinternals/downloads/process-explorer)
    to be able to see the Process IDs of running programs.

    Run Chrome with the sandbox disabled (otherwise the render process
    instrumentation doesn't get written to disk) and with a startup dialog
    for each renderer:

    ```shell
    out\instrument\chrome --no-sandbox --renderer-startup-dialog
    ```

    Note the Process IDs of the browser and render process (there is sometimes
    more than one; you want the one that loads the New Tab Page).

    Check in `\src\tmp\` for instrumentation output from those processes, for
    example `cygprofile_14652.txt` and `cygprofile_23592.txt`. The files are
    only written once a certain number of function calls have been made, so
    sometimes the renderer needs to be reloaded in order for the file to be
    produced.


1.  If the files appear to have sensible contents (a long list of function names
    that eventually seem related to what the browser and render process should
    do), copy them into `chrome\build\`:

    ```shell
    copy \src\tmp\cygprofile_25392.txt chrome\build\chrome.x64.orderfile
    copy \src\tmp\cygprofile_14652.txt chrome\build\chrome_child.x64.orderfile
    ```

1.  Re-build the `chrome` target. This will re-link `chrome.dll` and
    `chrome_child.dll` using the new order files and surface any link errors if
    the files are broken.

    ```shell
    ninja -C out\instrument chrome
    ```


1.  Repeat the previous steps with a 32-bit build, i.e. passing
    `target_cpu="x86"` to gn and storing the files as `.x86.orderfile`.


1.  Upload the order files to Google Cloud Storage. They will get downloaded
    by a `gclient` hook based on the contents of the `.orderfile.sha1` files.

    You need to have write access to the `chromium-browser-clang` GCS bucket
    for this step.

    ```shell
    cd chrome\build\
    upload_to_google_storage.py -b chromium-browser-clang/orderfiles -z orderfile chrome.x64.orderfile chrome.x86.orderfile chrome_child.x64.orderfile chrome_child.x86.orderfile
    gsutil.py setacl public-read gs://chromium-browser-clang/orderfiles/*
    ```


1.  Check in the `.sha1` files corresponding to the orderfiles created by the
    previous step.
