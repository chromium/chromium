// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines the `ScopedRefPtr` type, which is a Rust wrapper for a
//! `base::scoped_refptr`. Therefore, it only accepts types which match the
//! ref-counting behavior of `base::scoped_refptr`, and only works on opaque
//! C++ types.
//!
//! `ScopedRefPtr` relies heavily on the fact that opaque C++ values are
//! represented in Rust using zero-sized types (ZSTs), and it is therefore
//! permissible to have multiple mutable references, and to mutate the value
//! while a shared reference is held. This is because ZSTs take up no memory
//! (as far as Rust knows), and so their references cannot overlap or alias,
//! and thus it is impossible for them to violate Rust's aliasing rules.
//!
//! Of course, when used to represent an opaque C++ object, the pointed-to
//! object does take up space (Rust just doesn't know or care), so cxx exposes
//! mutable references behind a Pin. This is to prevent surprising behavior: for
//! example, `mem::swap`ing two opaque cxx objects is a no-op (because Rust has
//! no information about their memory layout).
//!
//! Since it's possible to have multiple mutable references to a ZST, care must
//! be taken when invoking functions that are not thread-safe. Such functions
//! should always be `unsafe` in Rust.

use std::ptr::NonNull;

/// Indicates an opaque C++ type with an internal mechanism for ref-counting.
///
/// # SAFETY:
/// The `impl` must guarantee that `T` can only be destroyed
/// when the last ref-count is given up by calling `Release`.
/// (For example, it must not be possible to allocate `T` on the stack.)
pub unsafe trait CxxRefCounted: cxx::ExternType<Kind = cxx::kind::Opaque> + 'static {
    /// Increments the object's ref-count. Safe to call, but can cause memory
    /// leaks if the object isn't appropriately `release`d later.
    ///
    /// Note that it's not legal to call this if the ref count is 0, but so
    /// long as the object is coming from C++, the ref count of the rust
    /// representation will always be at least 1.
    ///
    /// This type takes &self for compatibility with the C++ signature, and
    /// because increasing the ref-count doesn't logically mutate the object.
    fn add_ref(&self);

    /// Decrement the object's ref count. This may lead to the object being
    /// deallocated, so this function demands an exclusive reference. Logically,
    /// it should take `self` by value, but we want to call it from `drop`,
    /// which only provides a mutable reference.
    ///
    /// SAFETY: The caller must ensure that:
    /// 1. They own 1 of this object's ref-counts.
    /// 2. No code will dereference `self` after the call.
    unsafe fn release(&mut self);
}

/// A marker trait indicating that the implementation of CxxRefCounted for `T`
/// is thread-safe, and therefore it is safe to use `ScopedRefPtr<T>`
/// concurrently, provided it's safe to use `T` alone.
///
/// Note that thread-safety for `T` is more subtle for C++ types than in Rust.
/// `Send` and `Sync` should only be implemented for a type `T` after careful
/// inspection of its implementation. If a type is mostly thread-safe but has
/// some non-thread-safe methods, you may want to implement `Send` and `Sync`
/// and ensure that the non-thread-safe methods are marked `unsafe`.
pub unsafe trait CxxRefCountedThreadSafe: CxxRefCounted {}

/// A pointer to an object which manages its own ref count. The ref count impl
/// guarantees that the object will not be dropped while the pointer remains.
///
/// Safety implications:
///
/// 1. The contained pointer is always non-null and can be safely dereferenced
///    as long as the ScopedRefPtr is alive.
/// 2. This type DOES NOT guarantee exclusive access to the pointed-to object,
///    even if you have a mutable reference! The user is responsible for
///    ensuring any potential concurrent accesses are safe.
pub struct ScopedRefPtr<T: CxxRefCounted> {
    ptr: NonNull<T>,
}

impl<T: CxxRefCounted> ScopedRefPtr<T> {
    /// Create a new ScopedRefPtr from a pointer to a C++ value which was
    /// obtained from a C++ scoped_refptr.
    ///
    /// SAFETY: ptr must generated from a C++ scoped_refptr to T which gave up
    /// ownership without decrementing the ref count (e.g. by calling release())
    pub unsafe fn wrap_ref_counted(ptr: *mut T) -> Option<ScopedRefPtr<T>> {
        NonNull::new(ptr).map(|nonnull| ScopedRefPtr { ptr: nonnull })
    }

    /// Returns a pinned mutable reference to the stored object. Note that
    /// because T is opaque, this reference does _not_ ensure the object is not
    /// mutated while the reference exists, and multiple mutable references can
    /// co-exist.
    ///
    /// Note that this function takes &self, not &mut self, because it is valid
    /// for multiple mutable references to a zero-sized type to exist.
    ///
    /// Note as well that the pin here isn't really doing anything, because T
    /// is zero-sized. However, attempting to move out of the reference won't do
    /// anything, for the same reason, so the pin exists to prevent surprising
    /// behavior. The return value of this function should typically only be
    /// used as an argument to C++ FFI methods.
    pub fn as_pin(&self) -> std::pin::Pin<&mut T> {
        // SAFETY: For `as_ptr`, see the implementation of `Deref`.
        // For new_unchecked: this type won't move the underlying data anywhere.
        unsafe { std::pin::Pin::new_unchecked(&mut *self.ptr.as_ptr()) }
    }
}

impl<T: CxxRefCounted> Drop for ScopedRefPtr<T> {
    /// Decrement the object's ref-count before it goes out of scope.
    fn drop(&mut self) {
        // SAFETY: for `as_mut`, see the implementation of `Deref`.
        // We own one ref-count of this object (either from wrapping
        // a released pointer, or cloning an existing one), and we're dropping
        // it so we know it won't be referenced any more.
        unsafe { self.ptr.as_mut().release() };
    }
}

impl<T: CxxRefCounted> Clone for ScopedRefPtr<T> {
    /// Clone the pointer, incrementing the value's ref-count.
    fn clone(&self) -> Self {
        let mut cloned_ptr = self.ptr.clone();
        // SAFETY: see the implementation of `Deref`.
        unsafe { cloned_ptr.as_mut() }.add_ref();
        ScopedRefPtr { ptr: cloned_ptr }
    }
}

// Note that because T is opaque, this reference does _not_ ensure the object is
// not mutated while the reference exists.
impl<T: CxxRefCounted> std::ops::Deref for ScopedRefPtr<T> {
    type Target = T;

    fn deref(&self) -> &T {
        // SAFETY: The pointer is not null, points to valid data, and is zero-
        // sized, so we need not enforce aliasing rules.
        unsafe { self.ptr.as_ref() }
    }
}

// New scope so the `use std::fmt` doesn't pollute the rest of the file
const _: () = {
    use std::fmt::{Debug, Display, Error, Formatter};

    impl<T: CxxRefCounted + Debug> Debug for ScopedRefPtr<T> {
        fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), Error> {
            Debug::fmt(&**self, f)
        }
    }

    impl<T: CxxRefCounted + Display> Display for ScopedRefPtr<T> {
        fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), Error> {
            Display::fmt(&**self, f)
        }
    }
};

/// SAFETY it's safe to send/share T, and the CxxRefCountedThreadSafe
/// implementation guarantees that the ref-counting is itself thread-safe.
unsafe impl<T: Send + Sync + CxxRefCountedThreadSafe> Send for ScopedRefPtr<T> {}
unsafe impl<T: Send + Sync + CxxRefCountedThreadSafe> Sync for ScopedRefPtr<T> {}
