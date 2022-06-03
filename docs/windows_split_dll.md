# Windows Split DLLs

A build mode where chrome.dll is split into two separate DLLs. This was
undertaken as one possible workaround for toolchain limitations on Windows.

We removed support for this again after the toolchain limitations were fixed,
see https://crbug.com/726150.

## How

Split DLL used to be controlled by the `is_multi_dll_chrome` gn variable.

## Details

This forcible split was implemented by putting .lib files in either one DLL or
the other, and causing unresolved externals that result during linking to be
forcibly exported from the other DLL. This works relatively cleanly for function
import/export, however it cannot work for data export.

Some more details can be found on the initial commit of the `split_link` script
https://src.chromium.org/viewvc/chrome?revision=200049&view=revision and the
associated bugs: https://crbug.com/237249 https://crbug.com/237267.
