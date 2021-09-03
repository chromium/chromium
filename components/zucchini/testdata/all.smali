# Tests most/all DEX behaviors as of version 37.
# Disassembled from dexdump test files.
# Repo: https://android.googlesource.com/platform/art/
# File: test/dexdump/all.dex

# Compile using smali: https://github.com/JesusFreke/smali
# java -jar smali.jar assemble all.smali --api 25

.class public LA;
.super Ljava/lang/Object;


# static fields
.field private static sB:B

.field private static sC:C

.field private static sI:I

.field private static sJ:J

.field private static sO:LA;

.field private static sS:S

.field private static sZ:Z


# instance fields
.field private mB:B

.field private mC:C

.field private mI:I

.field private mJ:J

.field private mO:LA;

.field private mS:S

.field private mZ:Z


# direct methods
.method public constructor <init>()V
    .registers 1

    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    return-void
.end method

.method public static arrays()V
    .registers 3

    aget v0, v1, v2

    aget-wide v0, v1, v2

    aget-object v0, v1, v2

    aget-boolean v0, v1, v2

    aget-byte v0, v1, v2

    aget-char v0, v1, v2

    aget-short v0, v1, v2

    aput v0, v1, v2

    aput-wide v0, v1, v2

    aput-object v0, v1, v2

    aput-boolean v0, v1, v2

    aput-byte v0, v1, v2

    aput-char v0, v1, v2

    aput-short v0, v1, v2

    return-void
.end method

.method public static binary_ops()V
    .registers 3

    add-int v0, v1, v2

    sub-int v0, v1, v2

    mul-int v0, v1, v2

    div-int v0, v1, v2

    rem-int v0, v1, v2

    and-int v0, v1, v2

    or-int v0, v1, v2

    xor-int v0, v1, v2

    shl-int v0, v1, v2

    shr-int v0, v1, v2

    ushr-int v0, v1, v2

    add-long v0, v1, v2

    sub-long v0, v1, v2

    mul-long v0, v1, v2

    div-long v0, v1, v2

    rem-long v0, v1, v2

    and-long v0, v1, v2

    or-long v0, v1, v2

    xor-long v0, v1, v2

    shl-long v0, v1, v2

    shr-long v0, v1, v2

    ushr-long v0, v1, v2

    add-float v0, v1, v2

    sub-float v0, v1, v2

    mul-float v0, v1, v2

    div-float v0, v1, v2

    rem-float v0, v1, v2

    add-double v0, v1, v2

    sub-double v0, v1, v2

    mul-double v0, v1, v2

    div-double v0, v1, v2

    rem-double v0, v1, v2

    return-void
.end method

.method public static binary_ops_2addr()V
    .registers 2

    add-int/2addr v0, v1

    sub-int/2addr v0, v1

    mul-int/2addr v0, v1

    div-int/2addr v0, v1

    rem-int/2addr v0, v1

    and-int/2addr v0, v1

    or-int/2addr v0, v1

    xor-int/2addr v0, v1

    shl-int/2addr v0, v1

    shr-int/2addr v0, v1

    ushr-int/2addr v0, v1

    add-long/2addr v0, v1

    sub-long/2addr v0, v1

    mul-long/2addr v0, v1

    div-long/2addr v0, v1

    rem-long/2addr v0, v1

    and-long/2addr v0, v1

    or-long/2addr v0, v1

    xor-long/2addr v0, v1

    shl-long/2addr v0, v1

    shr-long/2addr v0, v1

    ushr-long/2addr v0, v1

    add-float/2addr v0, v1

    sub-float/2addr v0, v1

    mul-float/2addr v0, v1

    div-float/2addr v0, v1

    rem-float/2addr v0, v1

    add-double/2addr v0, v1

    sub-double/2addr v0, v1

    mul-double/2addr v0, v1

    div-double/2addr v0, v1

    rem-double/2addr v0, v1

    return-void
.end method

.method public static binary_ops_lit16()V
    .registers 2

    add-int/lit16 v0, v1, 0x1234

    rsub-int v0, v1, 0x1234

    mul-int/lit16 v0, v1, 0x1234

    div-int/lit16 v0, v1, 0x1234

    rem-int/lit16 v0, v1, 0x1234

    and-int/lit16 v0, v1, 0x1234

    or-int/lit16 v0, v1, 0x1234

    xor-int/lit16 v0, v1, 0x1234

    return-void
.end method

.method public static binary_ops_lit8()V
    .registers 2

    add-int/lit8 v0, v1, 0x12

    rsub-int/lit8 v0, v1, 0x12

    mul-int/lit8 v0, v1, 0x12

    div-int/lit8 v0, v1, 0x12

    rem-int/lit8 v0, v1, 0x12

    and-int/lit8 v0, v1, 0x12

    or-int/lit8 v0, v1, 0x12

    xor-int/lit8 v0, v1, 0x12

    shl-int/lit8 v0, v1, 0x12

    shr-int/lit8 v0, v1, 0x12

    ushr-int/lit8 v0, v1, 0x12

    return-void
.end method

.method public static compares()V
    .registers 3

    cmpl-float v0, v1, v2

    cmpg-float v0, v1, v2

    cmpl-double v0, v1, v2

    cmpg-double v0, v1, v2

    cmp-long v0, v1, v2

    return-void
.end method

.method public static conditionals()V
    .registers 2

    if-eq v0, v1, :cond_18

    if-ne v0, v1, :cond_18

    if-lt v0, v1, :cond_18

    if-ge v0, v1, :cond_18

    if-gt v0, v1, :cond_18

    if-le v0, v1, :cond_18

    if-eqz v0, :cond_18

    if-nez v0, :cond_18

    if-ltz v0, :cond_18

    if-gez v0, :cond_18

    if-gtz v0, :cond_18

    if-lez v0, :cond_18

    :cond_18
    return-void
.end method

.method public static constants()V
    .registers 1

    const/4 v0, 0x1

    const/16 v0, 0x1234

    const v0, 0x12345678

    const/high16 v0, 0x12340000

    const-wide/16 v0, 0x1234

    const-wide/32 v0, 0x12345678

    const-wide v0, 0x1234567890abcdefL    # 5.626349108908516E-221

    const-wide/high16 v0, 0x1234000000000000L

    const-string v0, "string"

    const-string/jumbo v0, "string"

    const-class v0, Ljava/lang/Object;

    return-void
.end method

.method public static misc()V
    .registers 5

    nop

    monitor-enter v0

    monitor-exit v0

    check-cast v0, Ljava/lang/Object;

    instance-of v0, v1, Ljava/lang/Object;

    array-length v0, v1

    new-instance v0, Ljava/lang/Object;

    new-array v0, v1, Ljava/lang/Object;

    filled-new-array {v0, v1, v2, v3, v4}, [Ljava/lang/Object;

    filled-new-array/range {v0 .. v4}, [Ljava/lang/Object;

    fill-array-data v0, :array_1e

    throw v0

    goto :goto_1c

    goto/16 :goto_1c

    goto/32 :goto_1c

    :goto_1c
    return-void

    nop

    :array_1e
    .array-data 4
        0x1
        0x2
        0x3
        0x4
        0x5
        0x6
        0x7
        0x8
        0x9
        0x0
    .end array-data
.end method

.method public static moves()V
    .registers 2

    move v0, v1

    move/from16 v0, v1

    move/16 v0, v1

    move-wide v0, v1

    move-wide/from16 v0, v1

    move-wide/16 v0, v1

    move-object v0, v1

    move-object/from16 v0, v1

    move-object/16 v0, v1

    move-result v0

    move-result-wide v0

    move-result-object v0

    move-exception v0

    return-void
.end method

.method public static packed_switch()V
    .registers 1

    packed-switch v0, :pswitch_data_8

    :goto_3
    return-void

    goto :goto_3

    :pswitch_5
    goto :goto_3

    :pswitch_6
    goto :goto_3

    nop

    :pswitch_data_8
    .packed-switch 0x7ffffffe
        :pswitch_5
        :pswitch_6
    .end packed-switch
.end method

.method public static return32()I
    .registers 1

    return v0
.end method

.method public static return64()I
    .registers 2

    return-wide v0
.end method

.method public static return_object()Ljava/lang/Object;
    .registers 1

    return-object v0
.end method

.method public static sparse_switch()V
    .registers 2

    sparse-switch v0, :sswitch_data_4

    :sswitch_3
    return-void

    :sswitch_data_4
    .sparse-switch
        0x1111 -> :sswitch_3
        0x2222 -> :sswitch_3
        0x3333 -> :sswitch_3
        0x4444 -> :sswitch_3
    .end sparse-switch
.end method

.method public static static_fields()V
    .registers 1

    sget v0, LA;->sI:I

    sget-wide v0, LA;->sJ:J

    sget-object v0, LA;->sO:LA;

    sget-boolean v0, LA;->sZ:Z

    sget-byte v0, LA;->sB:B

    sget-char v0, LA;->sC:C

    sget-short v0, LA;->sS:S

    sput v0, LA;->sI:I

    sput-wide v0, LA;->sJ:J

    sput-object v0, LA;->sO:LA;

    sput-boolean v0, LA;->sZ:Z

    sput-byte v0, LA;->sB:B

    sput-char v0, LA;->sC:C

    sput-short v0, LA;->mS:S

    return-void
.end method

.method public static unary_ops()V
    .registers 2

    neg-int v0, v1

    not-int v0, v1

    neg-long v0, v1

    not-long v0, v1

    neg-float v0, v1

    neg-double v0, v1

    int-to-long v0, v1

    int-to-float v0, v1

    int-to-double v0, v1

    long-to-int v0, v1

    long-to-float v0, v1

    long-to-double v0, v1

    float-to-int v0, v1

    float-to-long v0, v1

    float-to-double v0, v1

    double-to-int v0, v1

    double-to-long v0, v1

    double-to-float v0, v1

    int-to-byte v0, v1

    int-to-char v0, v1

    int-to-short v0, v1

    return-void
.end method


# virtual methods
.method public instance_fields()V
    .registers 2

    iget v0, p0, LA;->sI:I

    iget-wide v0, p0, LA;->sJ:J

    iget-object v0, p0, LA;->sO:LA;

    iget-boolean v0, p0, LA;->sZ:Z

    iget-byte v0, p0, LA;->sB:B

    iget-char v0, p0, LA;->sC:C

    iget-short v0, p0, LA;->sS:S

    iput v0, p0, LA;->sI:I

    iput-wide v0, p0, LA;->sJ:J

    iput-object v0, p0, LA;->sO:LA;

    iput-boolean v0, p0, LA;->sZ:Z

    iput-byte v0, p0, LA;->sB:B

    iput-char v0, p0, LA;->sC:C

    iput-short v0, p0, LA;->sS:S

    return-void
.end method

.method public invokes()V
    .registers 5

    invoke-virtual {v0, v1, v2, v3, p0}, LA;->invokes()V

    invoke-super {v0, v1, v2, v3, p0}, LA;->invokes()V

    invoke-direct {v0, v1, v2, v3, p0}, LA;->invokes()V

    invoke-static {v0, v1, v2, v3, p0}, LA;->invokes()V

    invoke-interface {v0, v1, v2, v3, p0}, LA;->invokes()V
.end method
