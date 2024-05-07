# Orderfile

## Generating Orderfiles Manually

To generate an orderfile you can run the `orderfile_generator_backend.py` script.

Example:
```
tools/cygprofile/orderfile_generator_backend.py --target-arch=arm64 --use-remoteexec
```

You can specify the architecture (arm or arm64) with `--target-arch`. For quick local testing you can use `--streamline-for-debugging`. To build using Reclient, use `--use-remoteexec` (Googlers only). There are several other options you can use to configure/debug the orderfile generation. Use the `-h` option to view the various options.

To build Chrome with a locally generated orderfile, use the `chrome_orderfile_path=<path_to_orderfile>` GN arg.