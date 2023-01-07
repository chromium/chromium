# Linux Cert Management

The easy way to manage certificates is navigate to chrome://settings/certificates.
Then click on the "Manage Certificates" button. This will load a built-in
interface for managing certificates.

On Linux, Chromium uses the
[NSS Shared DB](https://wiki.mozilla.org/NSS_Shared_DB_And_LINUX). If the
built-in manager does not work for you then you can configure certificates with
the
[NSS command line tools](http://www.mozilla.org/projects/security/pki/nss/tools/).

## Details

### Get the tools

*   Debian/Ubuntu: `sudo apt install libnss3-tools`
*   Fedora: `sudo dnf install nss-tools`
*   Gentoo: `su -c  "echo 'dev-libs/nss utils' >> /etc/portage/package.use &&
    emerge dev-libs/nss"` (You need to launch all commands below with the `nss`
    prefix, e.g., `nsscertutil`.)
*   Opensuse: `sudo zypper install mozilla-nss-tools`

### List all certificates

    certutil -d sql:$HOME/.pki/nssdb -L

### List details of a certificate

    certutil -d sql:$HOME/.pki/nssdb -L -n <certificate nickname>

### Add a certificate

```shell
certutil -d sql:$HOME/.pki/nssdb -A -t <TRUSTARGS> -n <certificate nickname> \
-i <certificate filename>
```

The TRUSTARGS are three strings of zero or more alphabetic characters, separated
by commas. They define how the certificate should be trusted for SSL, email, and
object signing, and are explained in the
[certutil docs](https://firefox-source-docs.mozilla.org/security/nss/legacy/tools/nss_tools_certutil/index.html)
or
[Meena's blog post on trust flags](https://web.archive.org/web/20131212024426/https://blogs.oracle.com/meena/entry/notes_about_trust_flags).

For example, to trust a root CA certificate for issuing SSL server certificates,
use

```shell
certutil -d sql:$HOME/.pki/nssdb -A -t "C,," -n <certificate nickname> \
-i <certificate filename>
```

To import an intermediate CA certificate, use

```shell
certutil -d sql:$HOME/.pki/nssdb -A -t ",," -n <certificate nickname> \
-i <certificate filename>
```

Note: to trust a self-signed server certificate, we should use

```
certutil -d sql:$HOME/.pki/nssdb -A -t "P,," -n <certificate nickname> \
-i <certificate filename>
```

#### Add a personal certificate and private key for SSL client authentication

Use the command:

    pk12util -d sql:$HOME/.pki/nssdb -i PKCS12_file.p12

to import a personal certificate and private key stored in a PKCS #12 file. The
TRUSTARGS of the personal certificate will be set to "u,u,u".

### Delete a certificate

    certutil -d sql:$HOME/.pki/nssdb -D -n <certificate nickname>
