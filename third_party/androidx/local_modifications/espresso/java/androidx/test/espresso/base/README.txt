Local modification to fix:
 * https://github.com/android/android-test/issues/2443

To find latest code:
 * https://cs.android.com/androidx/android-test/+/main:espresso/core/java/androidx/test/espresso/base/ViewHierarchyExceptionHandler.java;l=51?q=truncateExceptionMessage&sq=

To see local changes:
 * diff ViewHierarchyExceptionHandler.java.orig ViewHierarchyExceptionHandler.java

To update:
 * Hopefully any update will see the thread bug being fixed and this override can be removed.
 * Otherwise, diff against .orig to create a .patch file, then update the .orig, then apply the patch.
