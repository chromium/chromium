# Tests invoke-polymorphic added in DEX version 38.
# Disassembled from dexdump test files.
# Repo: https://android.googlesource.com/platform/art/
# File: test/dexdump/invoke-polymorphic.dex

# Compile using smali: https://github.com/JesusFreke/smali
# java -jar smali.jar assemble invoke-polymorphic.smali --api 28

.class public LMain;
.super Ljava/lang/Object;
.source "Main.java"


# direct methods
.method public constructor <init>()V
    .registers 1

    .prologue
    .line 9
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    return-void
.end method

.method public static main([Ljava/lang/String;)V
    .registers 10
    .param p0, "args"    # [Ljava/lang/String;
    .annotation system Ldalvik/annotation/Throws;
        value = {
            Ljava/lang/Throwable;
        }
    .end annotation

    .prologue
    const-wide v2, 0x400199999999999aL    # 2.2

    const/4 v4, 0x1

    .line 31
    const/4 v0, 0x0

    .line 32
    .local v0, "handle":Ljava/lang/invoke/MethodHandle;
    const/4 v5, 0x0

    .line 33
    .local v5, "o":Ljava/lang/Object;
    const-string/jumbo v1, "a"

    move v6, v4

    invoke-polymorphic/range {v0 .. v6}, Ljava/lang/invoke/MethodHandle;->invoke([Ljava/lang/Object;)Ljava/lang/Object;, (Ljava/lang/String;DILjava/lang/Object;I)Ljava/lang/String;

    move-result-object v7

    .line 34
    .local v7, "s":Ljava/lang/String;
    invoke-polymorphic {v0, v2, v3, v4}, Ljava/lang/invoke/MethodHandle;->invokeExact([Ljava/lang/Object;)Ljava/lang/Object;, (DI)I

    move-result v8

    .line 35
    .local v8, "x":I
    const-string/jumbo v1, "a"

    invoke-polymorphic {v0, v1, v2, v3, v4}, Ljava/lang/invoke/MethodHandle;->invoke([Ljava/lang/Object;)Ljava/lang/Object;, (Ljava/lang/String;DI)V

    .line 56
    return-void
.end method
