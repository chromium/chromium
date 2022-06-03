This service offers a way to perform proxy resolution using Windows APIs. This
enables several Windows proxy features that are not otherwise supported in
Chromium, including:
- Per-network interface proxy configurations
- Windows-like PAC script execution
- Windows-like WPAD devolution
- The [NRPT](https://docs.microsoft.com/en-us/previous-versions/windows/it-pro/windows-server-2012-R2-and-2012/dn593632(v=ws.11))
- [DirectAccess](https://docs.microsoft.com/en-us/windows-server/remote/remote-access/directaccess/directaccess)

See Design Doc for details:
https://docs.google.com/document/d/1yISJPmgdSppz4CMebU4yXA-HKYTOSbDdBnRj3YccjQE/edit?usp=sharing
