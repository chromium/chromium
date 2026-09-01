# Tracked Preferences

NOTE: This README is incomplete.

## Preference integrity validation

### Authenticators

Tracked preferences have their integrity validated by integrity metadata called
authenticators (also called "hashes" throughout this directory for legacy
reasons). Authenticators are written to a pref hash store when tracked
preferences have their value changed, and are read later at validation time.

Tracked preferences also use a "super authenticator", a single authenticator
covering all tracked preference values. Just like individual preference
authenticators, the super authenticator is updated when tracked preference
values change and is read at validation time.

#### Types of authenticators

Originally, tracked preferences were only validated with an HMAC (hash-based
message authentication code), which anyone can generate, including attackers
seeking to overwrite pref values. To provide better protection against pref
hijacking, tracked prefs are now encrypted with OS-provided encryption when
available, via //components/os_crypt, and the resulting encrypted hash is used
for validation. HMACs are still used as a fallback, but are now a legacy
authenticator.

When code in this directory is specific to one type of authenticator, it should
be marked as such (e.g., by including `Hmac` or `EncryptedHash` in method
names). Similarly, when code is generic to all types of authenticator, it should
use the umbrella term `Authenticator` (or `AuthData` when dealing with data
containing authenticators).
