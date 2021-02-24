# TPM Quick ref

TODO: this page looks very outdated. glossary.md does not exist,
git.chromium.org does not exist. Delete it?

This page is meant to help keep track of TPM use across the system. It may not
be up to date at any given point, but it's a wiki so you know what to do.

## Details

*   [TPM ownership management](http://git.chromium.org/gitweb/?p=chromiumos/platform/cryptohome.git;a=blob;f=README.tpm)
*   TPM_Clear is done (as in vboot_reference) but in the firmware code itself on
    switch between dev and verified modes and in recovery.  (TODO: link code)
*   [TPM owner password clearing](http://git.chromium.org/gitweb/?p=chromium/chromium.git;a=blob;f=chrome/browser/chromeos/login/login_utils.cc;h=9c4564e074c650bd91c27243c589d603740793bb;hb=HEAD#l861)
    (triggered at sign-in by chrome):
*   [PCR extend](http://git.chromium.org/gitweb/?p=chromiumos/platform/vboot_reference.git;a=blob;f=firmware/lib/tpm_bootmode.c)
    (no active use elsewhere):
*   [NVRAM use for OS rollback attack protection](http://git.chromium.org/gitweb/?p=chromiumos/platform/vboot_reference.git;a=blob;f=firmware/lib/rollback_index.c)
*   [Tamper evident storage](http://git.chromium.org/gitweb/?p=chromiumos/platform/cryptohome.git;a=blob;f=README.lockbox)
*   [Tamper-evident storage for avoiding runtime device management mode changes](http://git.chromium.org/gitweb/?p=chromium/chromium.git;a=blob;f=chrome/browser/ash/login/enrollment/enterprise_enrollment_screen.cc)
*   [User key/passphrase and cached data protection](http://git.chromium.org/gitweb/?p=chromiumos/platform/cryptohome.git;a=blob;f=README.homedirs)
*   A TPM in a Chrome device has an EK certificate that is signed by an
    intermediate certificate authority that is dedicated to the specific TPMs
    allocated for use in Chrome devices. OS-level self-validation of the
    platform TPM should be viable with this or chaining any other trust
    expectations.
*   TPM is used for per-user certificate storage (NSS+PKCS#11) using
    opencryptoki but soon to be replaced by chaps. Update links here when chaps
    stabilizes (Each user's pkcs#11 key store is kept in their homedir to ensure
    it is tied to the local user account). This functionality includes VPN and
    802.1x-related keypairs.
