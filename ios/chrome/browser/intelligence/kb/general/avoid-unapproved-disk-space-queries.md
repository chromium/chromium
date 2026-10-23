---
name: ios/avoid-unapproved-disk-space-queries
description: When querying free or total disk space on iOS, declare an approved reason in PrivacyInfo.xcprivacy and never transmit measurements off-device
rubric: safety
severity: should_fix
---

**Rule.** Do not query free or total disk space on iOS (via
`base::SysInfo::AmountOfFreeDiskSpace`, `statvfs`, `statfs`, `NSFileManager`, or
`NSURLVolumeAvailableCapacityKey`) without declaring an approved reason in the
application's privacy manifest (`PrivacyInfo.xcprivacy`) under
`NSPrivacyAccessedAPICategoryDiskSpace`. Never collect, log, or transmit disk
space measurements off-device, as Apple guidelines strictly prohibit using disk
space for user tracking or device fingerprinting. If disk space queries are
prototyped or non-essential, gate them behind build flags or avoid querying on
iOS.

**Why.** Apple App Store Review Guidelines restrict disk space APIs as Required
Reason APIs (`NSPrivacyAccessedAPICategoryDiskSpace`) to prevent device
fingerprinting. Querying disk space without declaring an authorized reason in
`PrivacyInfo.xcprivacy` or transmitting disk space measurements off-device
violates App Store policies and can cause app review rejection.

## Examples

Bad:

```cpp
// Querying free disk space on iOS without privacy manifest declaration:
void GetFreeDiskSpace(
    const base::FilePath& path,
    base::OnceCallback<void(std::optional<int64_t>)> callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&base::SysInfo::AmountOfFreeDiskSpace, path),
      std::move(callback));
}
```

Good:

```cpp
void GetFreeDiskSpace(
    const base::FilePath& path,
    base::OnceCallback<void(std::optional<int64_t>)> callback) {
#if BUILDFLAG(IS_IOS)
  // On iOS, querying disk space requires `NSPrivacyAccessedAPICategoryDiskSpace`
  // in `PrivacyInfo.xcprivacy`, an approved reason, and no off-device
  // transmission.
  std::move(callback).Run(std::nullopt);
#else
  // Query free disk space in the background to avoid blocking the caller.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&base::SysInfo::AmountOfFreeDiskSpace, path),
      std::move(callback));
#endif
}
```
