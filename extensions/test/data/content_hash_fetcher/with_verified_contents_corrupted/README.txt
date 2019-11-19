This archive contains:
  _metadata/verified_contents.json with a valid signature (see notes about
with_verified_contents archive, it's the same) and hash of one file,
background.js.
  background.js, which contents differs from the original one
  _metadata/computed_hashes.json, which contains hash of modified background.js
(so this hash effectively differs from one stored in verified_contents.json).

The purpose of this archive is to check that content verifier will catch such
type of corruption too.
