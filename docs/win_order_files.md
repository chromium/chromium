# Updating the Windows .order files

Since [#824529](https://crrev.com/824529), order files are no longer used for
linking Chromium on Windows. Instead, the linker orders the binary contents
based on profile-guided optimization (PGO) data, using LLD's call graph profile
sort feature. That provides similar benefits and uses use the existing PGO
infrastructure instead of requiring maintenance of the order files. See
[crbug.com/1077279](https://crbug.com/1077279).
