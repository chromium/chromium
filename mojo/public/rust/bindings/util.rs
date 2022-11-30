// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module contains some useful functions for encoding.

/// Given some size value, the size is aligned to some number in bytes.
///
/// Neither the size nor bytes may be zero (those are always aligned) and
/// bytes must be a power of two (other alignments don't make sense).
pub fn align_bytes(size: usize, bytes: usize) -> usize {
    debug_assert!(bytes != 0);
    debug_assert!((bytes & (!bytes + 1)) == bytes);
    (size + bytes - 1) & (!(bytes - 1))
}

/// Converts some number of bits into however many bytes are needed to
/// represent that bit size.
pub fn bits_to_bytes(bits: usize) -> usize {
    (bits + 7) >> 3
}

#[cfg(test)]
mod tests {
    use super::align_bytes;
    use super::bits_to_bytes;

    #[test]
    fn check_align_bytes() {
        assert_eq!(align_bytes(12, 8), 16);
        assert_eq!(align_bytes(16, 4), 16);
        assert_eq!(align_bytes(1, 1), 1);
    }

    #[test]
    #[should_panic]
    fn check_bad_align_bytes() {
        assert_eq!(align_bytes(15, 7), 21);
        assert_eq!(align_bytes(2, 0), 0);
    }

    #[test]
    fn check_bits_to_bytes() {
        assert_eq!(bits_to_bytes(8), 1);
        assert_eq!(bits_to_bytes(0), 0);
        assert_eq!(bits_to_bytes(1), 1);
        assert_eq!(bits_to_bytes(21), 3);
    }
}
