# Tests const-method-type added in DEX version 39.

# Compile using smali: https://github.com/JesusFreke/smali
# java -jar smali.jar assemble const-method-type.smali --api 28

.class public LConstMethodTypeTest;
.super Ljava/lang/Object;

.method public test(I)V
    .registers 4
    const-method-type v0, ()I
    const-method-type v1, (C)V
    const-method-type v2, (I)V
    const-method-type v3, (I)I
    return-void
.end method
