# Tests const-method-handle added in DEX version 39.

# Compile using smali: https://github.com/JesusFreke/smali
# java -jar smali.jar assemble const-method-handle.smali --api 28

.class public LConstMethodHandle;
.super Ljava/lang/Object;

.method public (I)V
  .registers 2
  const-method-handle v1, invoke-static@Ljava/lang/String;->copyValueOf([C)Ljava/lang/String;
  const-method-handle v0, invoke-instance@Ljava/lang/String;->charAt(I)C
  return-void
.end method
