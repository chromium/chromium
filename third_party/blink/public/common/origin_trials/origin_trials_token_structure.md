# Origin Trials Framework Token Structure

## Overview

The use of tokens is described fully in [Origin Trials Framework Design Outline](https://docs.google.com/document/d/1qVP2CK1lbfmtIJRIm6nwuEFFhGhYbtThLQPo3CSTtmg/edit#heading=h.gdjrm0kg6sg9). Briefly, tokens are generated for a specific combination of origin and experimental feature, known as an origin trial. The origin must present the trial token when intending to use a feature, which is then verified by the browser.  This document provides the details of token structure and validation.


## Token Format

Trial tokens are base64-encoded binary structures, containing a signed JSON-encoded string, as shown in the diagram below:
```
 _____________________________________
| Token version number (1 byte)       |
|‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾|
| Public Key Signature (64 bytes)     |
|_____________________________________|
| Payload Length (4 bytes, big-endian)|
|‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾|
| Origin                              |
| Feature name     <=== Payload       |
| ...more               (JSON string) |
|_____________________________________|
```

**Version** (1 byte): This should be the byte 0x02 or 0x03 (the preferred version), representing version 2 and 3, which this document describes. Version 2 is supported from M51 and Version 3 is supported from M86. There is no correlation to browser version.

**Signature**: (64 bytes): Ed25519(private_key, signed_data)

**Payload_length**: (4 bytes): Big-endian unsigned 32-bit integer representing the size of the payload field, in bytes.

**Payload**: The payload is a JSON string, encoding a dictionary which should contain these keys:


*   origin
*   isSubdomain [Optional]
    *   This key will often be omitted to minimize the size of the token
*   isThirdParty [Optional]
*   usage [Optional]
    *   This key should only be specified if **isThirdParty** is true
*   feature
*   expiry

**signed_data**: This is the concatenation of the version, payload length and payload fields.

**origin** = scheme + "://" + host + ":" + port

**isSubdomain** = true|false. Indicates if the token should match all subdomains of the given origin. When the key is omitted, that is equivalent to specifying value = false.

**isThirdParty** = true|false. Indicates if the token should match third party origins. When the key is omitted, that is equivalent to specifying value = false.

**usage** = ""|"subset". Indicates the usage restriction to be applied to the token. When the key is omitted, that is equivalent to specifying value = "".

**scheme** = the UTF-8 encoding of "https"|"http"|"chrome-extension"

**host** = the UTF-8 encoding of the hostname

**port** = the UTF-8 encoding of the port number in base-10 digits.

**feature** = A UTF-8 encoded string representing the name of an feature available for origin trials.

**expiry** = the UTC timestamp, as an integer number of seconds since the Unix epoch.


## Samples


### Regular Token on version 3

_This is a typical token, which is used for one exact domain._

Origin: https://example.com/

Feature Name: "Frobulate"

Expiry Date: 2020-12-31 23:59:59 (1609459199)

Payload (83 characters)

 `{"origin": "https://example.com:443", "feature": "Frobulate", "expiry": 1609459199}`

Signed Data: (88 bytes):


```
03 00 00 00 53 7b 22 6f 72 69 67 69 6e 22 3a 20 22 68 74 74 70 73 3a 2f 2f 65 78 61 6d 70 6c 65 2e 63 6f 6d 3a 34 34 33 22 2c 20 22 66 65 61 74 75 72 65 22 3a 20 22 46 72 6f 62 75 6c 61 74 65 22 2c 20 22 65 78 70 69 72 79 22 3a 20 31 36 30 39 34 35 39 31 39 39 7d
```


Signature: (Ed25519(private_key, data)) (64 bytes)


```
d6 13 93 95 8b 33 4b a1 5c f8 f6 38 4f dd 12 3f 26 11 d5 9b 71 63 af 0b 25 8c 35 d4 45 88 26 69 05 1b b2 9f 12 4c c7 58 bf 48 ee 69 45 3e d4 86 80 92 0d 38 9e a2 1f 6a 38 23 07 b1 cd b2 e7 0e
```


Complete Token: (152 bytes)


```
03 d6 13 93 95 8b 33 4b a1 5c f8 f6 38 4f dd 12 3f 26 11 d5 9b 71 63 af 0b 25 8c 35 d4 45 88 26 69 05 1b b2 9f 12 4c c7 58 bf 48 ee 69 45 3e d4 86 80 92 0d 38 9e a2 1f 6a 38 23 07 b1 cd b2 e7 0e 00 00 00 53 7b 22 6f 72 69 67 69 6e 22 3a 20 22 68 74 74 70 73 3a 2f 2f 65 78 61 6d 70 6c 65 2e 63 6f 6d 3a 34 34 33 22 2c 20 22 66 65 61 74 75 72 65 22 3a 20 22 46 72 6f 62 75 6c 61 74 65 22 2c 20 22 65 78 70 69 72 79 22 3a 20 31 36 30 39 34 35 39 31 39 39 7d
```


Trial token: (204 characters)


```
A9YTk5WLM0uhXPj2OE/dEj8mEdWbcWOvCyWMNdRFiCZpBRuynxJMx1i/SO5pRT7UhoCSDTieoh9qOCMHsc2y5w4AAABTeyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLmNvbTo0NDMiLCAiZmVhdHVyZSI6ICJGcm9idWxhdGUiLCAiZXhwaXJ5IjogMTYwOTQ1OTE5OX0=
```


Meta Tag:


```
<meta http-equiv="origin-trial" content="A9YTk5WLM0uhXPj2OE/dEj8mEdWbcWOvCyWMNdRFiCZpBRuynxJMx1i/SO5pRT7UhoCSDTieoh9qOCMHsc2y5w4AAABTeyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLmNvbTo0NDMiLCAiZmVhdHVyZSI6ICJGcm9idWxhdGUiLCAiZXhwaXJ5IjogMTYwOTQ1OTE5OX0=" />
```

### Subdomain Token on version 3

_This token is used for matching all subdomains of an origin (e.g. valid for *.example.com)._

Origin: https://example.com/

Matches Subdomains: Yes

Feature Name: "Frobulate"

Expiry Date: 2020-12-31 23:59:59 (1609459199)

Payload (104 characters)

 `{"origin": "https://example.com:443", "isSubdomain": true, "feature": "Frobulate", "expiry": 1609459199}`

Signed Data: (109 bytes):


```
 03 00 00 00 68 7b 22 6f 72 69 67 69 6e 22 3a 20 22 68 74 74 70 73 3a 2f 2f 65 78 61 6d 70 6c 65 2e 63 6f 6d 3a 34 34 33 22 2c 20 22 69 73 53 75 62 64 6f 6d 61 69 6e 22 3a 20 74 72 75 65 2c 20 22 66 65 61 74 75 72 65 22 3a 20 22 46 72 6f 62 75 6c 61 74 65 22 2c 20 22 65 78 70 69 72 79 22 3a 20 31 36 30 39 34 35 39 31 39 39 7d
```


Signature: (Ed25519(private_key, data)) (64 bytes)


```
31 e2 79 26 f7 35 71 d7 84 9d 73 bf 13 5c 99 47 91 df 9d 70 ce 55 f0 24 ae e9 60 cf 0c 08 57 cd 7a 7b e8 9d 98 b7 d2 44 b4 18 e2 96 52 57 88 c9 ab 6c ff 66 93 07 51 e8 88 29 8b 58 8b c6 e4 08
```


Complete Token: (173 bytes)


```
03 31 e2 79 26 f7 35 71 d7 84 9d 73 bf 13 5c 99 47 91 df 9d 70 ce 55 f0 24 ae e9 60 cf 0c 08 57 cd 7a 7b e8 9d 98 b7 d2 44 b4 18 e2 96 52 57 88 c9 ab 6c ff 66 93 07 51 e8 88 29 8b 58 8b c6 e4 08 00 00 00 68 7b 22 6f 72 69 67 69 6e 22 3a 20 22 68 74 74 70 73 3a 2f 2f 65 78 61 6d 70 6c 65 2e 63 6f 6d 3a 34 34 33 22 2c 20 22 69 73 53 75 62 64 6f 6d 61 69 6e 22 3a 20 74 72 75 65 2c 20 22 66 65 61 74 75 72 65 22 3a 20 22 46 72 6f 62 75 6c 61 74 65 22 2c 20 22 65 78 70 69 72 79 22 3a 20 31 36 30 39 34 35 39 31 39 39 7d


```


Trial token (232 characters):


```
AzHieSb3NXHXhJ1zvxNcmUeR351wzlXwJK7pYM8MCFfNenvonZi30kS0GOKWUleIyats/2aTB1HoiCmLWIvG5AgAAABoeyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLmNvbTo0NDMiLCAiaXNTdWJkb21haW4iOiB0cnVlLCAiZmVhdHVyZSI6ICJGcm9idWxhdGUiLCAiZXhwaXJ5IjogMTYwOTQ1OTE5OX0=
```


Meta Tag:


```
<meta http-equiv="origin-trial" content="AzHieSb3NXHXhJ1zvxNcmUeR351wzlXwJK7pYM8MCFfNenvonZi30kS0GOKWUleIyats/2aTB1HoiCmLWIvG5AgAAABoeyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLmNvbTo0NDMiLCAiaXNTdWJkb21haW4iOiB0cnVlLCAiZmVhdHVyZSI6ICJGcm9idWxhdGUiLCAiZXhwaXJ5IjogMTYwOTQ1OTE5OX0=" />
```


### Third Party Token (only supported in Version 3)

_This token is used for matching third party origins._

Origin: https://thirdparty.com/

Feature Name: "Frobulate"

Matches Third Party Origins: True

Expiry Date: 2020-12-31 23:59:59 (1609459199)

Payload (108 characters)

 `{"origin": "https://thirdparty.com:443", "feature": "Frobulate", "expiry": 1609459199, "isThirdParty": true}`

Signed Data: (113 bytes):


```
03 00 00 00 6c 7b 22 6f 72 69 67 69 6e 22 3a 20 22 68 74 74 70 73 3a 2f 2f 74 68 69 72 64 70 61 72 74 79 2e 63 6f 6d 3a 34 34 33 22 2c 20 22 69 73 54 68 69 72 64 50 61 72 74 79 22 3a 20 74 72 75 65 2c 20 22 66 65 61 74 75 72 65 22 3a 20 22 46 72 6f 62 75 6c 61 74 65 22 2c 20 22 65 78 70 69 72 79 22 3a 20 31 36 30 39 34 35 39 31 39 39 7d
```


Signature: (Ed25519(private_key, data)) (64 bytes)


```
1f 14 b0 25 3d 11 40 51 8f c3 d9 1b 5e 3b 70 e3 bb 56 a4 7c e8 11 75 dd 34 ae eb 0d b0 46 a8 b1 cc 9f 9f 11 45 0b bc e4 22 99 7c 16 97 51 13 60 27 c0 64 c9 6f bc 0a a8 15 80 7c 1f 1a ec 64 0e
```


Complete Token: (177 bytes)


```
03 1f 14 b0 25 3d 11 40 51 8f c3 d9 1b 5e 3b 70 e3 bb 56 a4 7c e8 11 75 dd 34 ae eb 0d b0 46 a8 b1 cc 9f 9f 11 45 0b bc e4 22 99 7c 16 97 51 13 60 27 c0 64 c9 6f bc 0a a8 15 80 7c 1f 1a ec 64 0e 00 00 00 6c 7b 22 6f 72 69 67 69 6e 22 3a 20 22 68 74 74 70 73 3a 2f 2f 74 68 69 72 64 70 61 72 74 79 2e 63 6f 6d 3a 34 34 33 22 2c 20 22 69 73 54 68 69 72 64 50 61 72 74 79 22 3a 20 74 72 75 65 2c 20 22 66 65 61 74 75 72 65 22 3a 20 22 46 72 6f 62 75 6c 61 74 65 22 2c 20 22 65 78 70 69 72 79 22 3a 20 31 36 30 39 34 35 39 31 39 39 7d
```


Trial token: (236 characters)


```
Ax8UsCU9EUBRj8PZG147cOO7VqR86BF13TSu6w2wRqixzJ+fEUULvOQimXwWl1ETYCfAZMlvvAqoFYB8HxrsZA4AAABseyJvcmlnaW4iOiAiaHR0cHM6Ly90aGlyZHBhcnR5LmNvbTo0NDMiLCAiaXNUaGlyZFBhcnR5IjogdHJ1ZSwgImZlYXR1cmUiOiAiRnJvYnVsYXRlIiwgImV4cGlyeSI6IDE2MDk0NTkxOTl9
```


Meta Tag:


```
<meta http-equiv="origin-trial" content="Ax8UsCU9EUBRj8PZG147cOO7VqR86BF13TSu6w2wRqixzJ+fEUULvOQimXwWl1ETYCfAZMlvvAqoFYB8HxrsZA4AAABseyJvcmlnaW4iOiAiaHR0cHM6Ly90aGlyZHBhcnR5LmNvbTo0NDMiLCAiaXNUaGlyZFBhcnR5IjogdHJ1ZSwgImZlYXR1cmUiOiAiRnJvYnVsYXRlIiwgImV4cGlyeSI6IDE2MDk0NTkxOTl9" />
```


### Third Party Token with alternative usage restriction (only supported in version 3)

_This token is used for matching third party origins. It also contains instructions for using alternative usage restrictions._

Origin: https://thirdparty.com/

Feature Name: "Frobulate"

Matches Third Party Origins: True

Usage Restrictions: Subset

Expiry Date: 2020-12-31 23:59:59 (1609459199)

Payload (127 characters)

 `{"origin": "https://thirdparty.com:443", "feature": "Frobulate", "expiry": 1609459199, "isThirdParty": true, "usage": "subset"}`

Signed Data: (132 bytes):


```
03 00 00 00 7f 7b 22 6f 72 69 67 69 6e 22 3a 20 22 68 74 74 70 73 3a 2f 2f 74 68 69 72 64 70 61 72 74 79 2e 63 6f 6d 3a 34 34 33 22 2c 20 22 69 73 54 68 69 72 64 50 61 72 74 79 22 3a 20 74 72 75 65 2c 20 22 75 73 61 67 65 22 3a 20 22 73 75 62 73 65 74 22 2c 20 22 66 65 61 74 75 72 65 22 3a 20 22 46 72 6f 62 75 6c 61 74 65 22 2c 20 22 65 78 70 69 72 79 22 3a 20 31 36 30 39 34 35 39 31 39 39 7d
```


Signature: (Ed25519(private_key, data)) (64 bytes)


```
31 2c ed 7c d0 1b 99 2d 58 5f e9 ba ba 12 53 94 73 c4 f1 1d 11 45 21 ab 05 ed 2f 68 48 b5 9a 09 53 46 e0 84 ac 1b b1 32 95 82 19 11 f7 91 87 49 f2 0d 4c 19 f1 c1 10 67 15 57 d8 18 92 e6 f0 05
```


Complete Token: (196 bytes)


```
03 31 2c ed 7c d0 1b 99 2d 58 5f e9 ba ba 12 53 94 73 c4 f1 1d 11 45 21 ab 05 ed 2f 68 48 b5 9a 09 53 46 e0 84 ac 1b b1 32 95 82 19 11 f7 91 87 49 f2 0d 4c 19 f1 c1 10 67 15 57 d8 18 92 e6 f0 05 00 00 00 7f 7b 22 6f 72 69 67 69 6e 22 3a 20 22 68 74 74 70 73 3a 2f 2f 74 68 69 72 64 70 61 72 74 79 2e 63 6f 6d 3a 34 34 33 22 2c 20 22 69 73 54 68 69 72 64 50 61 72 74 79 22 3a 20 74 72 75 65 2c 20 22 75 73 61 67 65 22 3a 20 22 73 75 62 73 65 74 22 2c 20 22 66 65 61 74 75 72 65 22 3a 20 22 46 72 6f 62 75 6c 61 74 65 22 2c 20 22 65 78 70 69 72 79 22 3a 20 31 36 30 39 34 35 39 31 39 39 7d
```


Trial token: (264 characters)


```
AzEs7XzQG5ktWF/puroSU5RzxPEdEUUhqwXtL2hItZoJU0bghKwbsTKVghkR95GHSfINTBnxwRBnFVfYGJLm8AUAAAB/eyJvcmlnaW4iOiAiaHR0cHM6Ly90aGlyZHBhcnR5LmNvbTo0NDMiLCAiaXNUaGlyZFBhcnR5IjogdHJ1ZSwgInVzYWdlIjogInN1YnNldCIsICJmZWF0dXJlIjogIkZyb2J1bGF0ZSIsICJleHBpcnkiOiAxNjA5NDU5MTk5fQ==
```


Meta Tag:


```
<meta http-equiv="origin-trial" content="AzEs7XzQG5ktWF/puroSU5RzxPEdEUUhqwXtL2hItZoJU0bghKwbsTKVghkR95GHSfINTBnxwRBnFVfYGJLm8AUAAAB/eyJvcmlnaW4iOiAiaHR0cHM6Ly90aGlyZHBhcnR5LmNvbTo0NDMiLCAiaXNUaGlyZFBhcnR5IjogdHJ1ZSwgInVzYWdlIjogInN1YnNldCIsICJmZWF0dXJlIjogIkZyb2J1bGF0ZSIsICJleHBpcnkiOiAxNjA5NDU5MTk5fQ==" />
```

### Version 2 Tokens
Version 2 Tokens will have same payload for regular and subdomain tokens, the only difference is the first version byte becomes \x02.

Third party tokens are only supported in Version 3.

# Obsolete Versions:

## Version 1

This was the original specification, which was supported in Chrome M50, but not used to support any actual experiments in that release.
