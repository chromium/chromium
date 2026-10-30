// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.lintsample

import android.util.Log
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember

/**
 * Not compiled: a Kotlin lint input. A [Sample] links to [SampleActivity.MODE_A] so that KDoc
 * references are resolved.
 */
data class Sample(val name: String, val count: Int = 0) {
  fun describe(): String =
    when {
      count > 1 -> "$name x$count"
      else -> name
    }

  companion object {
    const val TAG = "Sample"
  }
}

object Registry {
  private val items = mutableListOf<Sample>()

  fun add(sample: Sample) {
    items += sample
    Log.d(Sample.TAG, sample.describe())
  }

  fun names(): List<String> = items.map { it.name }.filter { it.isNotEmpty() }.sorted()
}

fun String.shout(): String = uppercase()

sealed interface Shape {
  data class Circle(val radius: Double) : Shape

  object Empty : Shape
}

fun area(shape: Shape): Double =
  when (shape) {
    is Shape.Circle -> Math.PI * shape.radius * shape.radius
    Shape.Empty -> 0.0
  }

@Composable
fun Counter(start: Int) {
  val remembered = remember { mutableStateOf(start) }
  val forgotten = mutableStateOf(0)
  Log.d(Sample.TAG, "${remembered.value} ${forgotten.value}")
}
