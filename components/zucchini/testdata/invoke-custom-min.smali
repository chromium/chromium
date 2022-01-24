# Tests invoke-custom added in DEX version 38.

# Compile using smali: https://github.com/JesusFreke/smali
# java -jar smali.jar assemble invoke-custom-min.smali --api 28

.class public LFoo;
.super Ljava/lang/Object;

.method public la1(Ljava/util/ArrayList;)V
    .registers 5
    .annotation system Ldalvik/annotation/Signature;
        value = {
            "(",
            "Ljava/util/ArrayList",
            "<",
            "Ljava/lang/String;",
            ">;)V"
        }
    .end annotation

    .prologue
    .line 42
    invoke-virtual {p1}, Ljava/util/ArrayList;->stream()Ljava/util/stream/Stream;

    move-result-object v0

    invoke-custom {}, call_site_1("bar", ()Ljava/util/function/Predicate;, (Ljava/lang/Object;)Z, invoke-static@LFoo;->lambda$la1$1(I)Z, (I)Z)@Ljava/lang/invoke/LambdaMetafactory;->metafactory(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;Ljava/lang/invoke/MethodType;Ljava/lang/invoke/MethodHandle;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/CallSite;

    move-result-object v1

    invoke-custom {}, call_site_2("test", ()Ljava/util/function/Predicate;, (Ljava/lang/Object;)Z, invoke-static@LFoo;->lambda$la1$1(Ljava/lang/String;)Z, (Ljava/lang/String;)Z)@Ljava/lang/invoke/LambdaMetafactory;->metafactory(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;Ljava/lang/invoke/MethodType;Ljava/lang/invoke/MethodHandle;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/CallSite;

    move-result-object v2

    invoke-interface {v0, v1, v2}, Ljava/util/stream/Stream;->filter(Ljava/util/function/Predicate;)Ljava/util/stream/Stream;

    .line 50
    return-void
.end method
