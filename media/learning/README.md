# Media Local Learning Framework

This directory contains code to support media's local learning experiment.
It provides lightweight learning algorithms that can be trained on the user's
local device to tailor media performance / behavior to match the user's usage.

## Terms

**Feature vector** - How we describe the "state of the world" to the learner.
  For example, we might describe a video using the features {width, height,
  format, frame rate}.

**Target** - The output value we'd like to predict, given some features.  We
  might want to know a boolean representing "will playback be smooth?".

**Training example** - A tuple {Feature vector, Target value} that demonstrates
  the desired target for some feature vector. The learning algorithm collects
  examples, and tries to generalize them to unseen features.

**Classification** - Class of problems for which the target value is nominal.
  For example, predicting the expected color from a set of five colors is
  a classification task.  The key idea is that the target values are unordered.

**Regression** - Class of problems for which the target value is numeric.  For
  example, predicting how tall a plant will grow is regression.

**Model** - A class of functions that relates features (inputs) to target values
  (outputs).  For example, a linear model relates them as:
  ```
  target = weight1 * feature1 + weight2 * feature2 + ...
  ```
  Note that the weights aren't known in advance; we'll choose them as part of
  the training process based on the training examples.

**Model parameters** - The missing values in our model that the learning
  learning algorithm tries to figure out based on the training data.  In our
  linear model, we'd need to know `weight1` and `weight2`.

**Learning task** - A problem we're trying to solve.  For example, "Will this
  video element be played before it's destroyed?"

## Classes

There are several classes that we define here.  While more detail can generally
be found in the header for the class, an overview of the main ones is:

**Learner** - Base class for a thing that knows how to convert training data
  into a fully trained model (model + parameters).  For example, we might have
  a Learner subclass that chooses the parameters for a Naive Bayes model.
  Similarly, we might have a Learner that trains a linear regression model.

**LearningTask** - Description of a task, and also, because it's convenient,
  a choice of model that will be used to learn it.  It contains:

    * name
    * description of features (name, nominal vs numeric, etc.)
    * description of the target value
    * description / parameters of the learning model to be used

**Instance** - Set of feature values.

**Value** - Representation of a number or (hashed) string.

## Models

All of our models are supervised.
