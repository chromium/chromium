// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import java.util.function.Consumer;

public class Playground {
    public static void accept(Consumer<Integer> consumer) {
        consumer.accept(5);
    }

    public static void main(String[] args) {
        accept(
                new Consumer<Integer>() {
                    @Override
                    public void accept(Integer a) {
                        System.out.println(a);
                    }
                });
    }
}
