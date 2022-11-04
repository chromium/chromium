// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_IMPL_FISHER_IRIS_DATASET_H_
#define MEDIA_LEARNING_IMPL_FISHER_IRIS_DATASET_H_

#include "media/learning/common/labelled_example.h"

namespace media {
namespace learning {

// Classic machine learning dataset.
//
// @misc{Dua:2017 ,
// author = "Dheeru, Dua and Karra Taniskidou, Efi",
// year = "2017",
// title = "{UCI} Machine Learning Repository",
// url = "http://archive.ics.uci.edu/ml",
// institution = "University of California, Irvine, "
//               "School of Information and Computer Sciences" }
class FisherIrisDataset {
 public:
  FisherIrisDataset();
  ~FisherIrisDataset();

  const TrainingData& GetTrainingData() const;

 private:
  TrainingData training_data_;
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_IMPL_FISHER_IRIS_DATASET_H_
